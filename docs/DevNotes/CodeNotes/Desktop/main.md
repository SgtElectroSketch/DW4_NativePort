# main.cpp

**File:** `native\desktop\src\main.cpp`

This is the entrypoint into the project.

## High-Level Control Flow

```text
main
    -> run
             -> locate and validate extracted assets
             -> initialize SDL
             -> create the window and renderer
             -> create the game state
             -> process events and advance fixed simulation frames
             -> clear and present the current frame
    -> report an error and return failure if an exception escapes
```

## SDL Entry-Point Configuration

First there is the preprocessor definition:

```cpp
#define SDL_MAIN_HANDLED
```

This definition tells SDL that we will handle the main function ourselves, rather than letting SDL define it.
SDL is a cross-platform library designed to provide low-level access to audio, keyboard, mouse, joystick, and graphics hardware via OpenGL and Direct3D. By defining `SDL_MAIN_HANDLED`, we take control of the `main` function, allowing us to initialize SDL and handle the program's entry point ourselves.

### Rendering API Background

OpenGL is a cross-platform graphics API that allows for the rendering of 2D and 3D vector graphics. It provides a set of functions to interact with the GPU, enabling high-performance graphics rendering. In the context of SDL, OpenGL can be used to create and manage graphics contexts for rendering within SDL windows.

Direct3D is a graphics API developed by Microsoft for rendering 3D graphics in applications where performance is important, such as games. It provides a set of functions to interact with the GPU, similar to OpenGL, but is specific to the Windows platform. In the context of SDL, Direct3D can be used as an alternative to OpenGL for creating and managing graphics contexts within SDL windows.

**Important distinction:** this file uses SDL's renderer API; it does not directly call OpenGL or Direct3D. SDL chooses
and manages an available rendering backend. OpenGL and Direct3D are useful background for understanding what may exist
under SDL, but they are not direct dependencies of this source file.

## Includes and Dependencies

### SDL

```cpp
#include <SDL3/SDL.h> // Include the SDL3 library for handling low-level access to audio, keyboard, mouse, joystick, and graphics hardware.
```

### Project Headers

```cpp
#include <dw4/content/AssetCatalog.hpp> // Include the AssetCatalog class for managing game assets.
#include <dw4/game/Game.hpp> // Include the Game class for managing the main game logic.
```

### C++ Standard Library

```cpp
#include <chrono> // Include the chrono library for handling time-related functions and measurements.
#include <cstdint> // Include the cstdint library for fixed-width integer types.
#include <filesystem> // Include the filesystem library for handling file system paths and operations.
#include <iostream> // Include the iostream library for input and output stream operations.
#include <memory> // Include the memory library for managing dynamic memory and smart pointers.
#include <stdexcept> // Include the stdexcept library for standard exception handling.
#include <string_view> // Include the string_view library for non-owning views of strings.
```

## Translation-Unit-Local Definitions

```cpp
namespace { // Anonymous namespace to limit the scope of the following definitions to this translation unit.
```

An anonymous namespace keeps its declarations private to this `.cpp` translation unit. Other source files cannot name
`SdlContext`, `asset_root_from`, `read_input`, or `run`, even though some members inside those definitions are public.

## SDL Lifetime: `SdlContext`

```cpp
class SdlContext final { // RAII wrapper for initializing and quitting SDL. RAII stands for Resource Acquisition Is Initialization, ensuring that SDL is properly initialized and cleaned up.
public: // We make it public so that instances of SdlContext can be created and used outside of this class.
    SdlContext()
    {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
            throw std::runtime_error(SDL_GetError());
        }
    }

    ~SdlContext()
    {
        SDL_Quit();
    }

    SdlContext(const SdlContext&) = delete;
    SdlContext& operator=(const SdlContext&) = delete;
};
```

**Scope clarification:** `public` makes the constructor and destructor accessible to code that can see the class. It
does not make the class visible outside this source file; the anonymous namespace controls that. `final` prevents
another class from inheriting from `SdlContext`.

Questions to answer while studying this block:

- What resource is acquired by the constructor?
- Why does constructor failure throw instead of leaving a partially initialized object?
- Why does the destructor not throw?
- Why are copying and copy assignment deleted?
- In what order will SDL, the renderer, and the window be destroyed?

### SDL Lifetime Answers

1. The constructor initializes the process-wide SDL video, audio, and gamepad subsystems selected by the three flags
    passed to `SDL_Init`. `SdlContext` does not contain an SDL pointer. It represents ownership of the obligation to
    call `SDL_Quit` after the program has finished using SDL.
2. A constructed `SdlContext` means that SDL is ready for use. Throwing on failure prevents the rest of `run` from
    creating windows or audio resources against an unusable SDL state. If a constructor throws, that object is never
    considered fully constructed, so its destructor does not run. Objects that were successfully constructed before it
    are still destroyed during stack unwinding.
3. Destructors should not allow exceptions to escape, especially while another exception is already unwinding the
    stack. `SDL_Quit` returns `void`, so cleanup has no recoverable result to report and the destructor can remain
    implicitly `noexcept`.
4. Copying would create two objects that both appear to own the same process-wide SDL lifetime. Both destructors would
    call `SDL_Quit`, and one copy could shut SDL down while the other was still in use. Deleting copy construction and
    copy assignment makes the single-owner intent explicit.
5. Local variables are destroyed in reverse construction order. In `run`, the renderer is destroyed first, then the
    window, then `SdlContext` calls `SDL_Quit`. This order is required because the renderer depends on the window, and
    both depend on initialized SDL subsystems. `Game` is declared after the renderer, so it is destroyed before the
    renderer; `catalog` was declared before `SdlContext`, so it is destroyed after SDL cleanup.

## Command-Line Asset Location: `asset_root_from`

```cpp
std::filesystem::path asset_root_from(int argument_count, char** arguments)
{
    std::filesystem::path root = "native/assets/generated";
    for (int index = 1; index < argument_count; ++index) {
        if (std::string_view(arguments[index]) == "--assets" && index + 1 < argument_count) {
            root = arguments[++index];
        } else {
            throw std::invalid_argument("usage: DW4.Desktop.exe [--assets <directory>]");
        }
    }
    return root;
}
```

Questions to answer while studying this block:

- What are `argument_count` and `arguments`?
- Why is the default path relative to the process working directory?
- Why is `std::string_view` sufficient for comparing an argument here?
- What happens for an unknown option, a missing `--assets` value, or multiple `--assets` options?
- Where is the returned path converted to an absolute normalized path?

### Asset-Path Answers

1. `argument_count` is the number of command-line strings supplied to the process. `arguments` points to an array of
    C-style character pointers. Element zero is conventionally the executable name, so parsing starts at index one.
2. `"native/assets/generated"` is intentionally a relative path so a normal development run can find assets under the
    repository root without embedding a developer-specific absolute path. The tradeoff is that the program's working
    directory must be the repository root unless `--assets` supplies another path.
3. `std::string_view` can compare the existing null-terminated argument without allocating or copying a new
    `std::string`. It does not own the characters, but the operating system-provided argument strings remain alive for
    the duration of `main`, which is longer than this comparison needs.
4. An unknown option or a final `--assets` without a following value throws `std::invalid_argument`. Multiple complete
    `--assets` options are accepted by this simple parser; each one overwrites `root`, so the last value wins. Incrementing
    `index` inside `root = arguments[++index]` consumes the value so the loop does not treat it as another option.
5. `AssetCatalog::load` converts the returned path with `std::filesystem::absolute(root).lexically_normal()`. This makes
    it absolute and removes lexical `.` and `..` components. It does not resolve symbolic links or prove that the path
    exists; opening the required JSON files performs the existence check.

## Keyboard-to-NES Input Mapping: `read_input`

```cpp
dw4::game::InputFrame read_input()
{
    const bool* keyboard = SDL_GetKeyboardState(nullptr);
    std::uint8_t buttons = 0;
    const auto add = [&buttons](const dw4::game::Button button) {
        buttons |= static_cast<std::uint8_t>(button);
    };

    if (keyboard[SDL_SCANCODE_Z]) add(dw4::game::Button::a);
    if (keyboard[SDL_SCANCODE_X]) add(dw4::game::Button::b);
    if (keyboard[SDL_SCANCODE_RSHIFT]) add(dw4::game::Button::select);
    if (keyboard[SDL_SCANCODE_RETURN]) add(dw4::game::Button::start);
    if (keyboard[SDL_SCANCODE_UP]) add(dw4::game::Button::up);
    if (keyboard[SDL_SCANCODE_DOWN]) add(dw4::game::Button::down);
    if (keyboard[SDL_SCANCODE_LEFT]) add(dw4::game::Button::left);
    if (keyboard[SDL_SCANCODE_RIGHT]) add(dw4::game::Button::right);
    return {buttons};
}
```

Questions to answer while studying this block:

- What is the lifetime and ownership of the pointer returned by `SDL_GetKeyboardState`?
- Why can multiple buttons fit inside one `std::uint8_t`?
- What does the lambda capture syntax `[&buttons]` mean?
- Why is `Button` converted with `static_cast<std::uint8_t>`?
- What does the bitwise OR assignment preserve when several keys are held?
- Does this function represent held state, pressed state, released state, or all three?

### Input-Mapping Answers

1. SDL owns the keyboard-state array returned by `SDL_GetKeyboardState`. The caller borrows the pointer and must not
    free it. SDL updates the array as events are processed, and the code only uses it immediately to build an owned
    `InputFrame` value.
2. The NES controller has eight buttons and `std::uint8_t` has eight bits. Each enumerator occupies one unique bit, so
    all simultaneous button combinations fit in one byte.
3. `[&buttons]` captures the local `buttons` variable by reference. Calling the lambda modifies the original byte rather
    than a copy. The parameter `button` is passed by value because the enum is only one byte.
4. `Button` is an `enum class`, which deliberately does not convert implicitly to its underlying integer type. The
    explicit `static_cast<std::uint8_t>` states that the bit value is being extracted intentionally.
5. Bitwise OR assignment sets the requested bit while preserving every bit already set. If Z and X are both held, the
    A and B bits coexist in the same byte rather than one assignment replacing the other.
6. This function captures held state for one simulation sample. It does not independently record transitions such as
    just-pressed or just-released. Those transitions will require comparing the current `InputFrame` with a previous
    frame in the input or game layer.

## Runtime Setup and Main Loop: `run`

### Load and Validate Assets

```cpp
int run(int argument_count, char** arguments)
{
    const auto catalog = dw4::content::AssetCatalog::load(asset_root_from(argument_count, arguments));
    const auto& summary = catalog.summary();
    std::cout << "assets: " << summary.dialogue_messages << " messages, "
              << summary.locations << " locations, " << summary.field_sprite_sets << " sprite sets, "
              << summary.monsters << " monsters, " << summary.music_tracks << " tracks, "
              << summary.sound_effects << " sound effects\n";
```

Questions to answer while studying this block:

- Why is `catalog` declared with `const auto`?
- Why is `summary` a reference rather than a copy?
- What validation occurs before SDL creates a window?
- What happens if an expected catalog is absent or malformed?

### Asset-Loading Answers

1. `const auto` asks the compiler to infer `AssetCatalog` from the factory's return type and prevents this local object
    from being modified after successful loading. The catalog is built completely inside `load` before it is returned.
2. `summary` is a const reference to the summary stored inside `catalog`, avoiding an unnecessary copy. The reference
    remains valid because `catalog` stays alive for the rest of `run` and is not moved or replaced.
3. Before SDL starts, `AssetCatalog::load` verifies that the required text, map, sprite, monster, audio, graphics, and
    screen JSON files exist and contain syntactically valid JSON. It also verifies that the specific top-level fields
    used for counts are arrays. It does not yet validate complete schemas, image dimensions, IDs, or cross-references.
4. A missing file causes `read_json` to throw `std::runtime_error`. Invalid JSON causes nlohmann-json's parse exception.
    A missing or incorrectly typed required array also causes `std::runtime_error`. These derive from `std::exception`,
    so `main` reports the message and exits with code one before any SDL window is created.

### Initialize SDL-Owned Resources

```cpp
    const SdlContext sdl;
    using Window = std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)>;
    using Renderer = std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)>;

    Window window(SDL_CreateWindow("Dragon Warrior IV Native", 768, 720, SDL_WINDOW_RESIZABLE), SDL_DestroyWindow);
    if (!window) {
        throw std::runtime_error(SDL_GetError());
    }

    Renderer renderer(SDL_CreateRenderer(window.get(), nullptr), SDL_DestroyRenderer);
    if (!renderer) {
        throw std::runtime_error(SDL_GetError());
    }
    if (!SDL_SetRenderLogicalPresentation(renderer.get(), dw4::game::logical_width, dw4::game::logical_height,
                                          SDL_LOGICAL_PRESENTATION_INTEGER_SCALE)) {
        throw std::runtime_error(SDL_GetError());
    }
```

Questions to answer while studying this block:

- Why do `SDL_Window` and `SDL_Renderer` need custom deleters?
- What does `decltype(&SDL_DestroyWindow)` produce?
- Why does `renderer` receive `window.get()` rather than ownership of the window?
- Why must the window outlive its renderer?
- Why is the physical window 768 by 720 while the logical presentation is 256 by 240?
- What behavior does integer scaling provide when the window is resized?

### SDL-Resource Answers

1. SDL window and renderer handles are opaque C resources created by SDL functions. They must be released with
    `SDL_DestroyWindow` and `SDL_DestroyRenderer`, not C++ `delete`. A `std::unique_ptr` with a custom deleter gives these
    C handles automatic single-owner cleanup.
2. `decltype(&SDL_DestroyWindow)` asks the compiler for the exact type of a pointer to that function. It is effectively
    the custom-deleter type required by `std::unique_ptr<SDL_Window, ...>` and avoids manually restating the signature.
3. `window.get()` returns a borrowed raw pointer without transferring ownership. SDL needs the handle to associate the
    renderer with the window, while the `Window` unique pointer remains responsible for destruction.
4. The renderer targets and depends on the window. Destroying the window first could leave the renderer referring to
    an invalid native window. Reverse local destruction naturally destroys `renderer` before `window`.
5. 768 by 720 is exactly three times the 256 by 240 logical canvas in each dimension. Game code can work in NES pixel
    coordinates while SDL maps those logical pixels to larger physical blocks.
6. `SDL_LOGICAL_PRESENTATION_INTEGER_SCALE` chooses whole-number scale factors so one logical pixel becomes an even
    square of physical pixels. Space that cannot fit another complete scale step is letterboxed instead of stretching
    pixel art unevenly.

### Establish Fixed-Step Simulation State

```cpp
    dw4::game::Game game;
    using Clock = std::chrono::steady_clock;
    constexpr double seconds_per_frame = 1.0 / dw4::game::frames_per_second;
    auto previous = Clock::now();
    double accumulated_seconds = 0.0;
    bool running = true;
```

Questions to answer while studying this block:

- Why is `steady_clock` appropriate for frame timing?
- How is `seconds_per_frame` derived from the NES frame rate?
- What is the accumulator storing?
- Which variables describe wall-clock time and which describe deterministic game state?

### Fixed-Step Timing Answers

1. `steady_clock` is monotonic: it is not adjusted when the system clock changes. Game timing needs elapsed duration,
    not calendar time, so clock corrections cannot make frame time move backward.
2. One frame lasts the reciprocal of frames per second. With `frames_per_second` equal to 60.0988, one simulation step
    is approximately 0.016639 seconds.
3. `accumulated_seconds` stores real elapsed time that has not yet been converted into fixed simulation ticks. Each tick
    consumes exactly `seconds_per_frame`, leaving any fractional remainder for the next outer-loop iteration.
4. `previous`, `now`, and `accumulated_seconds` describe wall-clock scheduling in `DW4.Desktop`. `Game::frame_` and the
    `InputFrame` passed to `tick` describe deterministic simulation state in `DW4.Game`. Keeping that boundary allows the
    same game ticks to be replayed without a real clock.

### Process Events and Advance the Game

```cpp
    while (running) {
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) {
                running = false;
            }
        }

        const auto now = Clock::now();
        accumulated_seconds += std::chrono::duration<double>(now - previous).count();
        previous = now;
        while (accumulated_seconds >= seconds_per_frame) {
            game.tick(read_input());
            accumulated_seconds -= seconds_per_frame;
        }
```

Questions to answer while studying this block:

- Why is event polling separate from keyboard-state sampling?
- What is the difference between closing the window and pressing Escape?
- Why can `game.tick` run zero, one, or several times during one outer-loop iteration?
- What happens after a long pause or debugger breakpoint?
- Why does fixed-step simulation improve deterministic replay and testing?

### Event-Loop Answers

1. `SDL_PollEvent` handles queued discrete events such as closing the window or pressing Escape. The keyboard-state
    array answers which gameplay keys are currently held. Window lifecycle and text/key transitions belong to events;
    controller snapshots belong to each simulation frame.
2. Both actions set `running` to false. `SDL_EVENT_QUIT` represents an operating-system or window-manager request,
    while Escape is an application-defined keyboard shortcut. The current outer iteration can still finish after the
    flag changes because there is no immediate `break` from the outer loop.
3. If less than one frame duration elapsed, no tick runs. Around one duration produces one tick. If rendering or the
    operating system delayed the process, several durations may be accumulated and the inner loop runs enough ticks to
    catch up.
4. A long debugger pause creates a very large accumulated duration. The current baseline will execute many catch-up
    ticks, all sampling the current keyboard state, because it has no maximum-frame clamp or pause reset yet. This is an
    important behavior to address before the production loop is complete.
5. Every game update receives one discrete input snapshot and advances exactly one logical frame, independent of render
    speed. A test or replay can therefore supply the same sequence of inputs and expect the same sequence of states.

### Present the Frame and Leave `run`

```cpp
        static_cast<void>(SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, SDL_ALPHA_OPAQUE));
        static_cast<void>(SDL_RenderClear(renderer.get()));
        static_cast<void>(SDL_RenderPresent(renderer.get()));
        SDL_Delay(1);
    }
    return 0;
}
```

Questions to answer while studying this block:

- Why are the SDL return values explicitly cast to `void`?
- What is the difference between clearing the back buffer and presenting it?
- Why is the current output only a black frame?
- Does `SDL_Delay(1)` define the simulation rate or merely reduce busy waiting?
- Which destructors run after this function returns?

### Presentation Answers

1. These SDL functions return status values marked as important by their declarations. Casting to `void` documents that
    the baseline intentionally ignores those particular results and avoids warnings under the strict build settings.
    Production rendering may choose to handle failures instead.
2. `SDL_RenderClear` fills the renderer's current back buffer with the selected draw color. `SDL_RenderPresent` makes
    that completed back buffer visible in the window. This separation prevents users from seeing a partially drawn frame.
3. No map, sprite, text, or UI draw commands exist yet. Clearing to opaque black is the complete current frame, so the
    window proves that setup and presentation work without pretending gameplay rendering is implemented.
4. `SDL_Delay(1)` merely yields for approximately one millisecond to reduce busy waiting. The elapsed `steady_clock`
    duration and fixed-step accumulator determine simulation ticks; the delay does not define the frame rate.
5. On normal return, local objects are destroyed in reverse order. `game` is destroyed first, followed by `renderer`,
    `window`, and `sdl`; `SdlContext` then calls `SDL_Quit`. Finally, earlier objects such as `catalog` are destroyed.
    Type aliases, references, arithmetic values, and raw borrowed pointers do not own resources requiring cleanup.

## Top-Level Error Boundary: `main`

```cpp
} // namespace

int main(int argument_count, char** arguments)
{
    try {
        return run(argument_count, arguments);
    } catch (const std::exception& exception) {
        std::cerr << "error: " << exception.what() << '\n';
        return 1;
    }
}
```

Questions to answer while studying this block:

- Why is `main` outside the anonymous namespace?
- Why is most startup logic delegated to `run`?
- Which exceptions are caught by `const std::exception&`?
- What do return codes `0` and `1` communicate to the operating system?
- What cleanup has already occurred before the exception message is printed?

### Error-Boundary Answers

1. The C++ runtime expects the program entry point to have external visibility. Keeping `main` outside the anonymous
    namespace provides that normal entry point, while implementation helpers remain private to the translation unit.
2. Delegating to `run` keeps the exception boundary and process return-code conversion small. `run` can use ordinary
    return values and RAII resources without manually cleaning up at every error branch.
3. It catches exceptions derived from `std::exception`, including `std::invalid_argument`, `std::runtime_error`, and
    nlohmann-json's standard exception hierarchy. It does not catch non-standard values thrown with `throw 5`, operating
    system termination, access violations, or other failures outside normal C++ exception handling.
4. Returning zero conventionally means success. Returning one means the program detected a failure. Shell scripts,
    debuggers, and parent processes can inspect this exit code even if they do not parse the printed message.
5. Stack unwinding destroys every fully constructed local in `run` before control reaches the catch block in `main`.
    Depending on where failure occurred, this includes the game, renderer, window, SDL context, and catalog. An object
    whose own constructor threw was never fully constructed and does not have its destructor called.
