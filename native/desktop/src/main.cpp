#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>

#include <dw4/content/AssetCatalog.hpp>
#include <dw4/game/Game.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {

class SdlContext final {
public:
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

int run(int argument_count, char** arguments)
{
    const auto catalog = dw4::content::AssetCatalog::load(asset_root_from(argument_count, arguments));
    const auto& summary = catalog.summary();
    std::cout << "assets: " << summary.dialogue_messages << " messages, "
              << summary.locations << " locations, " << summary.field_sprite_sets << " sprite sets, "
              << summary.monsters << " monsters, " << summary.music_tracks << " tracks, "
              << summary.sound_effects << " sound effects\n";

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

    dw4::game::Game game;
    using Clock = std::chrono::steady_clock;
    constexpr double seconds_per_frame = 1.0 / dw4::game::frames_per_second;
    auto previous = Clock::now();
    double accumulated_seconds = 0.0;
    bool running = true;

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

        static_cast<void>(SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, SDL_ALPHA_OPAQUE));
        static_cast<void>(SDL_RenderClear(renderer.get()));
        static_cast<void>(SDL_RenderPresent(renderer.get()));
        SDL_Delay(1);
    }
    return 0;
}

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