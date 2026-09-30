<!-- BEGIN AI-GENERATED CONTENT -->

# Game.hpp and Game.cpp

**Files:**

- `native\game\include\dw4\game\Game.hpp`
- `native\game\src\Game.cpp`

## Game Responsibility and Dependency Boundary

`DW4.Game` owns deterministic gameplay state and rules. It must not depend on SDL, operating-system APIs, filesystems,
JSON, rendering, audio devices, or the reference disassembly tools. The current baseline only proves the boundary: it
stores a frame number and one byte of controller state. Later field, battle, dialogue, event, menu, and save-domain
types will grow behind this interface.

## `Game.hpp`: Complete Baseline

```cpp
#pragma once

#include <cstdint>

namespace dw4::game {

inline constexpr std::uint32_t logical_width = 256;
inline constexpr std::uint32_t logical_height = 240;
inline constexpr double frames_per_second = 60.0988;

enum class Button : std::uint8_t {
        a = 1U << 0U,
        b = 1U << 1U,
        select = 1U << 2U,
        start = 1U << 3U,
        up = 1U << 4U,
        down = 1U << 5U,
        left = 1U << 6U,
        right = 1U << 7U,
};

struct InputFrame final {
        std::uint8_t buttons{};

        [[nodiscard]] bool is_pressed(Button button) const noexcept;
};

struct FrameState final {
        std::uint64_t number{};
        InputFrame input{};
};

class Game final {
public:
        void tick(InputFrame input) noexcept;
        [[nodiscard]] const FrameState& frame() const noexcept;

private:
        FrameState frame_{};
};

} // namespace dw4::game
```

## Header Protection and Fixed-Width Types

```cpp
#pragma once

#include <cstdint>
```

- `#pragma once` tells the compiler to process this header only once per translation unit, preventing duplicate
    declarations when several include paths lead back to it.
- `<cstdint>` supplies integer types with explicit widths. The controller mask is exactly eight bits, the dimensions
    are 32-bit unsigned values, and the frame counter is 64 bits regardless of the platform's ordinary `int` size.

## Namespace and Timing Constants

```cpp
namespace dw4::game {

inline constexpr std::uint32_t logical_width = 256;
inline constexpr std::uint32_t logical_height = 240;
inline constexpr double frames_per_second = 60.0988;
```

- `dw4::game` prevents generic names such as `Game` and `Button` from colliding with other libraries or domains.
- `constexpr` makes each value usable at compile time and guarantees it cannot change at runtime.
- A namespace-scope variable in a header needs `inline` so every translation unit may include the same definition
    without violating C++'s one-definition rule.
- 256 by 240 is the logical NES presentation canvas. The desktop renderer may enlarge it, but game coordinates remain
    in this stable logical space.
- 60.0988 is the intended NTSC simulation cadence. It is an approximation suitable for the current baseline; timing
    that affects behavior should ultimately be verified against reference evidence.

## Controller Bitmask

```cpp
enum class Button : std::uint8_t {
        a = 1U << 0U,
        b = 1U << 1U,
        select = 1U << 2U,
        start = 1U << 3U,
        up = 1U << 4U,
        down = 1U << 5U,
        left = 1U << 6U,
        right = 1U << 7U,
};
```

- `enum class` creates a strongly typed enumeration. `Button::a` does not implicitly become an integer or mix with an
    unrelated enum.
- `: std::uint8_t` fixes the underlying representation at one byte.
- `1U << n` shifts a single unsigned bit into a unique position. The values are 1, 2, 4, 8, 16, 32, 64, and 128.
- These are flags rather than sequential IDs. Several flags can be ORed together to represent simultaneous input.
- This mirrors the useful shape of an NES controller sample without requiring the native game to emulate controller
    ports or serial hardware reads.

## Input Value Object

```cpp
struct InputFrame final {
        std::uint8_t buttons{};

        [[nodiscard]] bool is_pressed(Button button) const noexcept;
};
```

- A `struct` defaults to public members, which is appropriate for this small value object.
- `final` prevents inheritance; input frames are data values, not a polymorphic hierarchy.
- `buttons{}` value-initializes the byte to zero, meaning no buttons are held in a default frame.
- `is_pressed` provides the bit-test operation in one place instead of spreading casts and masks through game code.
- The trailing `const` means the method does not modify the `InputFrame` on which it is called.
- `noexcept` promises that the bit test cannot throw an exception.
- `[[nodiscard]]` asks the compiler to warn if a caller computes the answer and ignores it.

## Per-Frame State

```cpp
struct FrameState final {
        std::uint64_t number{};
        InputFrame input{};
};
```

- `number` records how many simulation ticks have completed. A 64-bit counter would take millions of years to wrap at
    approximately 60 ticks per second.
- `input` stores the sample used by the most recently completed tick.
- Both members use `{}` initialization, so a default `FrameState` is frame zero with no buttons held.
- This is only scaffolding. The complete game will own domain state beyond one frame record rather than placing all
    gameplay data into this struct.

## Game Ownership and Public Contract

```cpp
class Game final {
public:
        void tick(InputFrame input) noexcept;
        [[nodiscard]] const FrameState& frame() const noexcept;

private:
        FrameState frame_{};
};
```

- A `class` defaults to private access. The public section exposes operations, while `frame_` can only be changed by
    `Game` methods.
- `tick` accepts an `InputFrame` by value because it is only one byte and logically represents a self-contained sample.
- `frame` returns a const reference, avoiding a copy while preventing callers from directly changing internal state.
- The returned reference remains valid while the `Game` object exists and `frame_` has not been replaced by an
    operation that invalidates references. The current implementation updates the same object in place.
- `Game final` communicates that this state machine is used through composition, not subclass customization.
- `frame_{}` establishes a valid deterministic initial state without requiring a user-written constructor.

## `Game.cpp`: Complete Baseline

```cpp
#include <dw4/game/Game.hpp>

namespace dw4::game {

bool InputFrame::is_pressed(const Button button) const noexcept
{
        return (buttons & static_cast<std::uint8_t>(button)) != 0;
}

void Game::tick(const InputFrame input) noexcept
{
        ++frame_.number;
        frame_.input = input;
}

const FrameState& Game::frame() const noexcept
{
        return frame_;
}

} // namespace dw4::game
```

## Header Inclusion and Namespace

```cpp
#include <dw4/game/Game.hpp>

namespace dw4::game {
```

- The implementation includes its own public header first. If the header lacks a dependency needed by its declarations,
    compiling this file will expose that mistake instead of accidentally relying on another include.
- Angle brackets work because the project adds `native/game/include` as an include root. Callers use the stable public
    path `dw4/game/Game.hpp` rather than knowing the header's absolute filesystem location.
- Reopening `dw4::game` lets definitions use `InputFrame`, `Button`, and `Game` without repeating full qualification.

## Testing a Button Bit

```cpp
bool InputFrame::is_pressed(const Button button) const noexcept
{
        return (buttons & static_cast<std::uint8_t>(button)) != 0;
}
```

- `static_cast` extracts the enum's one-byte flag value explicitly.
- Bitwise AND keeps only bits set in both the stored mask and requested flag.
- Comparing with zero converts that masked integer into a clear Boolean result.
- The implementation works for the declared single-button values. Passing an artificially combined `Button` value
    would answer whether any shared bit is set, not whether all combined bits are set.

## Advancing One Deterministic Tick

```cpp
void Game::tick(const InputFrame input) noexcept
{
        ++frame_.number;
        frame_.input = input;
}
```

- One call always increments the logical frame exactly once. Wall-clock duration is deliberately absent.
- The new input sample becomes the input associated with the completed frame.
- The current order increments first and stores input second, but no observer can see the object between these lines in
    this single-threaded baseline.
- Future game systems will update inside this operation or functions called by it. They must remain deterministic for
    the same prior state and input.
- `noexcept` is valid now because integer increment and byte assignment do not throw. If future tick logic allocates or
    uses throwing operations, the guarantee must be preserved deliberately or removed rather than violated.

## Read-Only State Access

```cpp
const FrameState& Game::frame() const noexcept
{
        return frame_;
}
```

- Returning by const reference exposes observation without transferring ownership or allowing mutation.
- The method's trailing `const` permits calls through a const `Game` reference.
- The caller must not keep this reference beyond the lifetime of its `Game` object.
- This accessor is useful for rendering and tests, but later architecture may expose narrower snapshots or render data
    rather than granting every consumer access to all game state.

<!-- END AI-GENERATED CONTENT -->
