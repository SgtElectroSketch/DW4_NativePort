#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>

#include "GraphicsAssets.hpp"
#include "GraphicsRenderer.hpp"
#include "TitleAudio.hpp"
#include "Input.hpp"
#include "AdventureStore.hpp"
#include "AdventureView.hpp"
#include "FieldView.hpp"
#include "GameplaySession.hpp"

#include <dw4/content/AssetCatalog.hpp>
#include <dw4/content/ReplayFile.hpp>
#include <dw4/content/TitleData.hpp>
#include <dw4/game/Game.hpp>
#include <dw4/game/Replay.hpp>
#include <dw4/game/Title.hpp>
#include <dw4/game/AdventureLog.hpp>
#include <dw4/game/MenuInput.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <charconv>
#include <stdexcept>
#include <string_view>
#include <iomanip>
#include <sstream>
#include <queue>
#include <array>

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

std::filesystem::path default_asset_root()
{
    if (const auto* base = SDL_GetBasePath()) {
        auto executable_directory = std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(base)));
        if (executable_directory.filename().empty()) {
            executable_directory = executable_directory.parent_path();
        }
        const auto development_assets = executable_directory.parent_path().parent_path() / "native" / "assets" / "generated";
        if (std::filesystem::is_directory(development_assets)) {
            return development_assets;
        }
        const auto adjacent_assets = executable_directory / "assets";
        if (std::filesystem::is_directory(adjacent_assets)) {
            return adjacent_assets;
        }
    }
    return "native/assets/generated";
}

struct RuntimeOptions final {
    std::filesystem::path asset_root = default_asset_root();
    bool validate_assets{};
    std::optional<std::filesystem::path> replay;
    std::optional<std::filesystem::path> render_check;
    std::optional<std::filesystem::path> title_check;
    std::uint32_t title_frame{1556};
    std::optional<std::uint32_t> run_frames;
    std::optional<std::filesystem::path> save_directory;
    std::optional<std::filesystem::path> menu_check;
    std::optional<std::filesystem::path> log_smoke;
    std::optional<std::filesystem::path> input_script;
    std::optional<std::filesystem::path> field_check;
    std::optional<std::filesystem::path> gameplay_smoke;
};

RuntimeOptions options_from(int argument_count, char** arguments)
{
    RuntimeOptions options;
    for (int index = 1; index < argument_count; ++index) {
        if (std::string_view(arguments[index]) == "--assets" && index + 1 < argument_count) {
            options.asset_root = arguments[++index];
        } else if (std::string_view(arguments[index]) == "--validate-assets") {
            options.validate_assets = true;
        } else if (std::string_view(arguments[index]) == "--replay" && index + 1 < argument_count) {
            options.replay = std::filesystem::path(arguments[++index]);
        } else if (std::string_view(arguments[index]) == "--render-check" && index + 1 < argument_count) {
            options.render_check = std::filesystem::path(arguments[++index]);
        } else if (std::string_view(arguments[index]) == "--title-check" && index + 1 < argument_count) {
            options.title_check = std::filesystem::path(arguments[++index]);
        } else if (std::string_view(arguments[index]) == "--title-frame" && index + 1 < argument_count) {
            const std::string_view value(arguments[++index]);
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), options.title_frame);
            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || options.title_frame == 0 ||
                options.title_frame > 1000000) {
                throw std::invalid_argument("--title-frame requires an integer between 1 and 1000000");
            }
        } else if (std::string_view(arguments[index]) == "--run-frames" && index + 1 < argument_count) {
            const std::string_view value(arguments[++index]);
            std::uint32_t frames = 0;
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), frames);
            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || frames == 0 || frames > 100000) {
                throw std::invalid_argument("--run-frames requires an integer between 1 and 100000");
            }
            options.run_frames = frames;
        } else if (std::string_view(arguments[index]) == "--save-dir" && index + 1 < argument_count) {
            options.save_directory = std::filesystem::path(arguments[++index]);
        } else if (std::string_view(arguments[index]) == "--menu-check" && index + 1 < argument_count) {
            options.menu_check = std::filesystem::path(arguments[++index]);
        } else if (std::string_view(arguments[index]) == "--log-smoke" && index + 1 < argument_count) {
            options.log_smoke = std::filesystem::path(arguments[++index]);
        } else if (std::string_view(arguments[index]) == "--input-script" && index + 1 < argument_count) {
            options.input_script = std::filesystem::path(arguments[++index]);
        } else if (std::string_view(arguments[index]) == "--field-check" && index + 1 < argument_count) {
            options.field_check = std::filesystem::path(arguments[++index]);
        } else if (std::string_view(arguments[index]) == "--gameplay-smoke" && index + 1 < argument_count) {
            options.gameplay_smoke = std::filesystem::path(arguments[++index]);
        } else {
            throw std::invalid_argument("usage: DW4.Desktop.exe [--assets <directory>] [--validate-assets | --replay <file> | --render-check <png>]");
        }
    }
    if ((options.validate_assets && (options.replay || options.render_check)) || (options.replay && options.render_check)) {
        throw std::invalid_argument("asset validation, replay, and render inspection are separate commands");
    }
    if (options.title_check && (options.validate_assets || options.replay || options.render_check)) {
        throw std::invalid_argument("title inspection is a separate command");
    }
    return options;
}

dw4::game::Color title_color(const dw4::content::TitleColor& color)
{
    return {color[0], color[1], color[2], color[3]};
}

void draw_title(dw4::desktop::GraphicsRenderer& renderer, const dw4::content::TitleData& data,
                const dw4::game::TitleState& state, const std::uint32_t sheet)
{
    const dw4::content::TitleFrame* frame = nullptr;
    if (state.phase == dw4::game::TitlePhase::finished) {
        renderer.begin({0, 0, 0, 255});
        return;
    }
    if (state.phase == dw4::game::TitlePhase::fade_out) {
        frame = &data.exit_frames[std::min<std::size_t>(state.fade_frame, data.exit_frames.size() - 1U)];
    } else {
        const auto current = state.frame == 0 ? std::uint64_t{0} : state.frame - 1U;
        const auto index = current < data.frames.size() ? current : data.loop_start + (current - data.loop_start) % data.loop_frames;
        frame = &data.frames[static_cast<std::size_t>(index)];
    }
    renderer.begin(title_color(frame->background));
    for (const auto& tile : frame->draws) {
        dw4::game::Palette palette;
        for (std::size_t index = 0; index < palette.size(); ++index) {
            palette[index] = title_color(data.palettes[tile.palette][index]);
        }
        renderer.draw({dw4::game::GraphicKind::tile_sheet, sheet, tile.tile, tile.x, tile.y, palette,
                       (tile.flips & 1U) != 0, (tile.flips & 2U) != 0, {0, tile.clip_top, 256, tile.clip_height}});
    }
}

dw4::game::FrameState game_snapshot(const dw4::content::ReplaySnapshotRecord& snapshot)
{
    dw4::game::FrameState state;
    state.number = snapshot.frame;
    state.input.buttons = snapshot.input;
    state.pressed.buttons = snapshot.pressed;
    state.released.buttons = snapshot.released;
    state.random = {snapshot.random_engine, snapshot.random_draw_counter};
    return state;
}

int run_replay_file(const std::filesystem::path& path)
{
    const auto document = dw4::content::ReplayFile::load(path);
    dw4::game::Replay replay;
    replay.initial = game_snapshot(document.initial);
    replay.frames.reserve(document.frames.size());
    for (const auto& frame : document.frames) {
        replay.frames.push_back({{frame.input}, frame.random_draws});
    }
    replay.checkpoints.reserve(document.checkpoints.size());
    for (const auto& checkpoint : document.checkpoints) {
        replay.checkpoints.push_back({checkpoint.frame_index, game_snapshot(checkpoint.snapshot)});
    }
    const auto result = dw4::game::run_replay(replay);
    if (result.difference) {
        std::cerr << "replay divergence at frame " << result.difference->frame
                  << ", field " << result.difference->field << '\n';
        return 2;
    }
    std::cout << "replay verified: " << replay.frames.size() << " input frames, " << replay.checkpoints.size()
              << " checkpoints; final frame " << result.final_state.number
              << ", random draws " << result.final_state.random.draw_counter << '\n';
    return 0;
}

std::array<dw4::game::LogSlot, 3> log_slots(const dw4::desktop::AdventureStore& store)
{
    const auto stored = store.inspect();
    std::array<dw4::game::LogSlot, 3> slots;
    for (std::size_t slot = 0; slot < slots.size(); ++slot) {
        slots[slot].corrupt = !stored[slot].error.empty();
        if (stored[slot].save) {
            const auto& save = *stored[slot].save;
            slots[slot] = {true, false, save.name, save.gender, save.message_speed, save.chapter};
        }
    }
    return slots;
}

std::optional<std::uint8_t> execute_log_request(dw4::game::AdventureLog& menu, dw4::desktop::AdventureStore& store, const dw4::game::LogRequest& request,
                                               const dw4::game::FrameState& frame)
{
    std::string error;
    std::optional<std::uint8_t> enter;
    try {
        switch (request.operation) {
        case dw4::game::LogOperation::create: {
            const auto save = dw4::desktop::GameplaySession::create_save(request, frame);
            store.write(request.destination, save);
            enter = request.destination;
            break;
        }
        case dw4::game::LogOperation::copy: store.copy(request.source, request.destination); break;
        case dw4::game::LogOperation::erase: store.erase(request.source); break;
        case dw4::game::LogOperation::speed: {
            const auto stored = store.inspect();
            if (!stored[request.source].save) throw std::runtime_error("Adventure log cannot be loaded.");
            auto save = *stored[request.source].save;
            save.message_speed = request.speed;
            store.write(request.source, save);
            break;
        }
        case dw4::game::LogOperation::load: {
            const auto stored = store.inspect();
            if (!stored[request.source].save || stored[request.source].save->chapter != 1) throw std::runtime_error("Adventure cannot be resumed by the Chapter 1 engine.");
            enter = request.source;
            break;
        }
        }
    } catch (const std::exception& failure) {
        error = failure.what();
    }
    menu.complete(log_slots(store), std::move(error));
    return enter;
}

std::uint8_t smoke_route_direction(const dw4::content::FieldData& data, const dw4::game::FieldState& state)
{
    const auto& position = state.adventure.position;
    const auto& map = data.map(position.map, position.submap);
    const auto count = static_cast<std::size_t>(map.width) * map.height;
    const auto start = static_cast<std::size_t>(position.y) * map.width + position.x;
    std::vector<int> previous(count, -1);
    std::vector<std::uint8_t> first(count);
    std::queue<std::size_t> pending;
    pending.push(start); previous[start] = static_cast<int>(start);
    constexpr std::array<int, 4> dx{0, 1, 0, -1}, dy{-1, 0, 1, 0};
    constexpr std::array<std::uint8_t, 4> buttons{16, 128, 32, 64};
    while (!pending.empty()) {
        const auto index = pending.front(); pending.pop();
        const int x = static_cast<int>(index % map.width), y = static_cast<int>(index / map.width);
        for (std::size_t direction = 0; direction < 4; ++direction) {
            const int next_x = x + dx[direction], next_y = y + dy[direction];
            if (next_x < 0 || next_y < 0 || next_x >= map.width || next_y >= map.height) {
                if (map.boundary_destination) return index == start ? buttons[direction] : first[index];
                continue;
            }
            const auto next = static_cast<std::size_t>(next_y) * map.width + next_x;
            if (previous[next] != -1 || map.tiles[map.cells[next] & 31U].behavior >= 128 ||
                std::ranges::any_of(state.actors, [&](const auto& actor) { return actor.active && actor.x == next_x && actor.y == next_y; })) continue;
            previous[next] = static_cast<int>(index);
            first[next] = index == start ? buttons[direction] : first[index];
            for (const auto& connection : map.connections) if (connection.x == next_x && connection.y == next_y && connection.map == 255) return first[next];
            pending.push(next);
        }
    }
    return 0;
}

int run(int argument_count, char** arguments)
{
    auto options = options_from(argument_count, arguments);
    const auto catalog = dw4::content::AssetCatalog::load(options.asset_root);
    const auto& summary = catalog.summary();
    std::cout << "assets: " << summary.dialogue_messages << " messages, "
              << summary.locations << " locations, " << summary.field_sprite_sets << " sprite sets, "
              << summary.monsters << " monsters, " << summary.music_tracks << " tracks, "
              << summary.sound_effects << " sound effects\n";

    if (options.validate_assets) {
        const auto graphics = dw4::desktop::GraphicsAssets::load(catalog);
        std::cout << "graphics verified: " << graphics.tiles.size() << " tile sheets, " << graphics.fonts.size() << " fonts\n";
        return 0;
    }
    if (options.replay) {
        return run_replay_file(*options.replay);
    }

    const SdlContext sdl;
    using Window = std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)>;
    using Renderer = std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)>;

    Window window(SDL_CreateWindow("Dragon Warrior IV Native", 768, 720,
                                   (options.render_check || options.title_check || options.menu_check || options.log_smoke || options.field_check || options.gameplay_smoke) ? SDL_WINDOW_HIDDEN : SDL_WINDOW_RESIZABLE), SDL_DestroyWindow);
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

    auto graphics = dw4::desktop::GraphicsAssets::load(catalog);
    std::optional<dw4::content::TitleData> title_data;
    if (!options.render_check) {
        title_data = dw4::content::TitleData::load(catalog.root() / "title");
        graphics.tiles.push_back(dw4::desktop::GraphicsAssets::load_title_patterns(title_data->image, title_data->tiles));
    }
    dw4::desktop::GraphicsRenderer graphics_renderer(renderer.get(), graphics);
    if (options.field_check) {
        const auto data = dw4::content::FieldData::load(catalog.root() / "opening");
        const auto sheet = static_cast<std::uint32_t>(graphics.tiles.size());
        graphics.tiles.push_back(dw4::desktop::GraphicsAssets::load_title_patterns(catalog.root() / "opening" / data.image(), data.tile_count()));
        const dw4::desktop::FieldView view(catalog, data, sheet);
        dw4::game::FieldState state;
        state.adventure = dw4::game::new_adventure({37}, 0, 3);
        dw4::game::Field field(view.rules(), state);
        view.draw(graphics_renderer, field.state());
        graphics_renderer.save_png(*options.field_check);
        std::cout << "native field rendered: map 2 submap 1; actors " << field.state().actors.size() << '\n';
        return 0;
    }
    if (options.render_check) {
        graphics_renderer.begin({24, 32, 40, 255});
        const dw4::game::Palette palette{{{0, 0, 0, 0}, {240, 244, 248, 255}, {64, 176, 144, 255}, {224, 96, 112, 255}}};
        for (std::uint32_t font_index = 0; font_index < catalog.font_sheets().size(); ++font_index) {
            const auto& font = catalog.font_sheets()[font_index];
            for (const auto& glyph : font.glyphs) {
                const auto position = static_cast<std::uint32_t>(glyph.tile);
                graphics_renderer.draw({dw4::game::GraphicKind::font, font_index, position,
                                        static_cast<std::int32_t>((position % 16U) * 8U + 8U),
                                        static_cast<std::int32_t>((position / 16U) * 8U + 8U + font_index * 88U), palette});
            }
        }
        for (std::uint32_t tile = 0; tile < 16 && !catalog.tile_sheets().empty(); ++tile) {
            graphics_renderer.draw({dw4::game::GraphicKind::tile_sheet, 0, tile, static_cast<std::int32_t>(tile * 8U + 8U),
                                    200, palette, tile % 2U != 0, tile % 4U >= 2});
        }
        graphics_renderer.save_png(*options.render_check);
        graphics_renderer.finish();
        std::cout << "native logical render saved: " << options.render_check->string() << '\n';
        return 0;
    }

    dw4::game::Title title({title_data->artwork_frame, title_data->fade_frames});
    const auto title_sheet = static_cast<std::uint32_t>(graphics.tiles.size() - 1U);
    if (options.title_check) {
        for (std::uint32_t frame = 0; frame < options.title_frame; ++frame) {
            title.tick({});
        }
        draw_title(graphics_renderer, *title_data, title.state(), title_sheet);
        graphics_renderer.save_png(*options.title_check);
        std::cout << "native title rendered at frame " << title.state().frame << '\n';
        return 0;
    }

    const dw4::desktop::AdventureView adventure_view(catalog);
    if (options.menu_check) {
        dw4::game::AdventureLog menu({}, adventure_view.name_characters(), false);
        adventure_view.draw(graphics_renderer, menu);
        graphics_renderer.save_png(*options.menu_check);
        return 0;
    }
    std::filesystem::path save_directory;
    if (options.save_directory) save_directory = *options.save_directory;
    else {
        const std::unique_ptr<char, decltype(&SDL_free)> preferred(SDL_GetPrefPath("NativePort", "DragonWarrior4"), SDL_free);
        if (!preferred) throw std::runtime_error(SDL_GetError());
        save_directory = std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(preferred.get()))) / "saves";
    }
    dw4::desktop::AdventureStore adventure_store(save_directory);
    if (options.gameplay_smoke) {
        if (!options.save_directory || std::ranges::any_of(adventure_store.inspect(), [](const auto& slot) { return slot.save || !slot.error.empty(); })) {
            throw std::invalid_argument("gameplay smoke requires an explicit empty save directory");
        }
        const auto data = dw4::content::FieldData::load(catalog.root() / "opening");
        const auto atlas = static_cast<std::uint32_t>(graphics.tiles.size());
        graphics.tiles.push_back(dw4::desktop::GraphicsAssets::load_title_patterns(catalog.root() / "opening" / data.image(), data.tile_count()));
        dw4::game::Game game;
        const dw4::game::LogRequest create{dw4::game::LogOperation::create, 0, 0, {37}, 0, 0};
        const auto initial = dw4::desktop::GameplaySession::create_save(create, game.frame());
        adventure_store.write(0, initial);
        dw4::desktop::GameplaySession session(catalog, data, atlas, initial, 0, adventure_store);
        session.draw(graphics_renderer);
        auto intro_image = *options.gameplay_smoke;
        intro_image.replace_filename(intro_image.stem().string() + "-intro.png");
        graphics_renderer.save_png(intro_image);
        const auto step = [&](const std::uint8_t input) { game.tick({input}); session.tick(game); };
        for (std::uint32_t presses = 0; presses < 100 && session.state().phase != 0; ++presses) { step(1); step(0); }
        if (session.state().phase != 0 || session.dialogue_active()) throw std::runtime_error("gameplay smoke did not complete original opening messages");
        const auto messages = session.validate_actor_dialogues();
        std::cout << "actor dialogue coverage verified: " << messages << " branches\n";
        bool town = false;
        for (std::uint32_t frames = 0; frames < 3000 && session.state().adventure.position.map != 255; ++frames) {
            step(smoke_route_direction(data, session.state()));
            const auto& position = session.state().adventure.position;
            if (position.map == 2 && position.submap == 0) town = true;
            if (session.dialogue_active()) { step(0); step(1); step(0); }
        }
        if (!town || session.state().adventure.position.map != 255) {
            const auto& position = session.state().adventure.position;
            throw std::runtime_error("gameplay smoke route stopped at map " + std::to_string(position.map) + " floor " + std::to_string(position.submap) +
                                     " tile " + std::to_string(position.x) + "," + std::to_string(position.y));
        }
        if (session.battle_active()) throw std::runtime_error("world-entry smoke unexpectedly started a battle before route verification");
        session.draw(graphics_renderer);
        graphics_renderer.save_png(*options.gameplay_smoke);
        session.save(game.frame());
        const auto stored = adventure_store.inspect()[0].save;
        if (!stored || !stored->world || stored->world->map != 255) throw std::runtime_error("world-map state was not persisted");
        dw4::desktop::GameplaySession resumed(catalog, data, atlas, *stored, 0, adventure_store);
        if (resumed.state() != session.state()) throw std::runtime_error("full gameplay state did not survive reload");
        const auto initial_gold = session.state().adventure.gold;
        const auto initial_experience = session.state().adventure.characters[6].experience;
        bool gained_gold = false, rendered_battle = false;
        std::uint32_t movement = 0;
        constexpr std::array<std::uint8_t, 4> walk{128, 32, 64, 16};
        for (std::uint32_t frames = 0; frames < 12000 && session.state().adventure.characters[6].level < 2; ++frames) {
            if (session.dialogue_active() || session.battle_active()) {
                if (session.battle_active() && !rendered_battle) {
                    session.draw(graphics_renderer);
                    auto image = *options.gameplay_smoke;
                    image.replace_filename(image.stem().string() + "-battle.png");
                    graphics_renderer.save_png(image);
                    rendered_battle = true;
                }
                step(1); step(0);
            } else if (session.state().adventure.position.map != 255) step(smoke_route_direction(data, session.state()));
            else {
                step(walk[(movement / 27U) % walk.size()]);
                ++movement;
            }
            gained_gold = gained_gold || session.state().adventure.gold > initial_gold;
        }
        if (!rendered_battle || !gained_gold || session.state().adventure.characters[6].experience <= initial_experience ||
            session.state().adventure.characters[6].level < 2) throw std::runtime_error("playable encounter reward/level route did not complete");
        session.save(game.frame());
        const auto reward_save = adventure_store.inspect()[0].save;
        if (!reward_save || !reward_save->world || reward_save->world->characters[6].level < 2) throw std::runtime_error("level/reward state was not persisted");
        std::cout << "playable battle rewards verified: level " << static_cast<int>(session.state().adventure.characters[6].level)
                  << ", EXP " << session.state().adventure.characters[6].experience << ", gold " << session.state().adventure.gold << '\n';
        std::cout << "gameplay smoke passed: opening, all actor dialogue branches, castle town world, full-state reload; frame " << game.frame().number << '\n';
        return 0;
    }
    dw4::game::AdventureLog adventure_menu(log_slots(adventure_store), adventure_view.name_characters(), false);
    if (options.log_smoke) {
        if (!options.save_directory || std::ranges::any_of(adventure_store.inspect(), [](const auto& slot) { return slot.save || !slot.error.empty(); })) {
            throw std::invalid_argument("log smoke requires an explicit empty save directory");
        }
        const auto confirm = [&]() { adventure_menu.tick({.confirm = true}); };
        const auto capture = [&](const std::string_view suffix) {
            adventure_view.draw(graphics_renderer, adventure_menu);
            auto path = *options.log_smoke;
            if (!suffix.empty()) path.replace_filename(path.stem().string() + "-" + std::string(suffix) + ".png");
            graphics_renderer.save_png(path);
        };
        capture("commands");
        confirm(); capture("slots"); confirm(); capture("");
        confirm();
        adventure_menu.tick({.right = true}); confirm();
        adventure_menu.tick({.right = true}); confirm();
        for (std::size_t step = 2; step < adventure_menu.characters().size() + 1U; ++step) adventure_menu.tick({.right = true});
        confirm(); capture("gender"); confirm(); capture("speed"); confirm(); capture("confirmation"); confirm();
        const auto create = adventure_menu.take_request();
        if (!create || create->operation != dw4::game::LogOperation::create) throw std::runtime_error("smoke creation did not reach storage");
        dw4::game::FrameState initial;
        initial.random.engine.seed(73);
        static_cast<void>(execute_log_request(adventure_menu, adventure_store, *create, initial));
        const auto saved = adventure_store.inspect()[0].save;
        if (!saved || saved->name.size() != 3) throw std::runtime_error("smoke name creation failed");
        adventure_store.copy(0, 1);
        dw4::desktop::AdventureStore restarted(save_directory);
        if (restarted.inspect()[1].save != saved) throw std::runtime_error("smoke restart/copy did not preserve the saved state");
        restarted.erase(1);
        if (restarted.inspect()[1].save) throw std::runtime_error("smoke erase did not empty the slot");
        std::cout << "adventure log smoke passed: name entry, create, copy, restart, erase\n";
        return 0;
    }
    dw4::game::MenuInput menu_input;
    dw4::desktop::Input input;
    std::optional<dw4::content::ReplayDocument> scripted_input;
    if (options.input_script) {
        scripted_input = dw4::content::ReplayFile::load(*options.input_script);
        if (scripted_input->frames.empty() || scripted_input->frames.size() > 100000) {
            throw std::invalid_argument("live input script must contain 1 through 100000 frames");
        }
        options.run_frames = static_cast<std::uint32_t>(scripted_input->frames.size());
    }
    bool showing_logs = false;
    std::optional<dw4::content::FieldData> field_data;
    std::unique_ptr<dw4::desktop::GameplaySession> session;
    std::uint32_t field_atlas = 0;
    std::cout << "adventure logs: " << adventure_store.directory().string() << '\n';
    dw4::game::Game game;
    dw4::desktop::TitleAudio title_audio;
    std::size_t music_index = 0;
    using Clock = std::chrono::steady_clock;
    constexpr double seconds_per_frame = 1.0 / dw4::game::frames_per_second;
    auto previous = Clock::now();
    double accumulated_seconds = 0.0;
    bool running = true;

    while (running) {
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            input.handle(event);
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) {
                running = false;
            }
        }

        const auto now = Clock::now();
        accumulated_seconds += std::chrono::duration<double>(now - previous).count();
        previous = now;
        while (accumulated_seconds >= seconds_per_frame && (!options.run_frames || game.frame().number < *options.run_frames)) {
            const auto sampled = scripted_input ? dw4::game::InputFrame{scripted_input->frames[static_cast<std::size_t>(game.frame().number)].input} :
                                                   input.sample();
            game.tick(sampled);
            if (session) {
                session->tick(game);
                if (session->leaving()) {
                    session.reset();
                    adventure_menu = dw4::game::AdventureLog(log_slots(adventure_store), adventure_view.name_characters(), true);
                    showing_logs = true;
                    menu_input.reset(sampled);
                }
            } else if (!showing_logs) {
                title.tick(sampled);
                if (title.state().phase == dw4::game::TitlePhase::finished) {
                    showing_logs = true;
                    adventure_menu = dw4::game::AdventureLog(log_slots(adventure_store), adventure_view.name_characters(), true);
                    menu_input.reset(sampled);
                    title_audio.stop();
                    std::cout << "adventure log entered at frame " << game.frame().number << '\n';
                }
            } else {
                adventure_menu.tick(menu_input.tick(sampled));
                if (adventure_menu.state().page == dw4::game::LogPage::return_to_title) {
                    title = dw4::game::Title({title_data->artwork_frame, title_data->fade_frames});
                    adventure_menu = dw4::game::AdventureLog(log_slots(adventure_store), adventure_view.name_characters(), true);
                    showing_logs = false;
                    music_index = 0;
                    title_audio.stop();
                    menu_input.reset(sampled);
                }
                if (const auto request = adventure_menu.take_request()) {
                    if (const auto slot = execute_log_request(adventure_menu, adventure_store, *request, game.frame())) {
                        if (!field_data) {
                            field_data = dw4::content::FieldData::load(catalog.root() / "opening");
                            field_atlas = static_cast<std::uint32_t>(graphics.tiles.size());
                            graphics.tiles.push_back(dw4::desktop::GraphicsAssets::load_title_patterns(catalog.root() / "opening" / field_data->image(), field_data->tile_count()));
                        }
                        const auto stored = adventure_store.inspect()[*slot].save;
                        if (!stored) throw std::runtime_error("selected adventure disappeared before gameplay entry");
                        auto frame = game.frame();
                        frame.random = {stored->random_engine, stored->random_draws};
                        game = dw4::game::Game(std::move(frame));
                        session = std::make_unique<dw4::desktop::GameplaySession>(catalog, *field_data, field_atlas, *stored, *slot, adventure_store);
                        std::cout << "native gameplay entered: slot " << static_cast<int>(*slot + 1) << '\n';
                    }
                    menu_input.reset(sampled);
                }
            }
            if (!session && !showing_logs && music_index < title_data->music.size() && title.state().frame >= title_data->music[music_index].frame) {
                std::ostringstream filename;
                filename << "track-" << std::setw(2) << std::setfill('0') << title_data->music[music_index].track << ".wav";
                try {
                    title_audio.play(catalog.root() / "audio" / "music" / filename.str());
                    std::cout << "title music started at frame " << title.state().frame << '\n';
                } catch (const std::exception& error) {
                    std::cerr << "audio unavailable: " << error.what() << '\n';
                }
                ++music_index;
            }
            accumulated_seconds -= seconds_per_frame;
        }

        if (session) session->draw(graphics_renderer);
        else if (showing_logs) adventure_view.draw(graphics_renderer, adventure_menu);
        else draw_title(graphics_renderer, *title_data, title.state(), title_sheet);
        graphics_renderer.finish();
        if (options.run_frames && game.frame().number >= *options.run_frames) {
            std::cout << "native run completed: " << game.frame().number << " frames\n";
            running = false;
        }
        SDL_Delay(1);
    }
    if (session) session->save(game.frame());
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