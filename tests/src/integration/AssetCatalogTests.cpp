#include <dw4/content/AssetCatalog.hpp>
#include <dw4/content/ReplayFile.hpp>
#include <dw4/content/TitleData.hpp>
#include <dw4/content/AdventureSave.hpp>
#include <dw4/content/FieldData.hpp>
#include <dw4/game/Replay.hpp>

#include "GraphicsAssets.hpp"
#include "GraphicsRenderer.hpp"
#include "Input.hpp"
#include "AdventureStore.hpp"
#include "AdventureView.hpp"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/catch_test_macros.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace {

class TemporaryAssets final {
public:
    TemporaryAssets()
        : root_(std::filesystem::temp_directory_path() /
                ("dw4-native-assets-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())))
    {
        std::filesystem::create_directories(root_);
    }

    ~TemporaryAssets()
    {
        std::error_code error;
        std::filesystem::remove_all(root_, error);
    }

    TemporaryAssets(const TemporaryAssets&) = delete;
    TemporaryAssets& operator=(const TemporaryAssets&) = delete;

    void write(const std::filesystem::path& relative_path, const std::string_view json) const
    {
        const auto path = root_ / relative_path;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream output(path);
        output << json;
    }

    void write_catalogs() const
    {
        write("text/dialogue.json", R"({"schema_version":1,"messages":[{"id":0,"group":0,"index":0,"symbols":[1,2]},
                                {"id":1,"group":0,"index":1,"symbols":[3]}]})");
        write("maps/index.json", R"({"schema_version":1,"locations":[{}]})");
        write("sprites/field/index.json", R"({"schema_version":1,"sets":[{},{},{}]})");
        write("monsters/index.json", R"({"schema_version":1,"monsters":[{},{},{},{}]})");
        write("audio/index.json", R"({"schema_version":1,"music":[{},{}],"sfx":[{},{},{}]})");
        write("graphics/index.json", R"({"schema_version":1,"chr":[{"image":"tiles-a.png","format":"2bpp","tiles":3},
                                      {"image":"tiles-b.png","format":"1bpp","tiles":1}],
                         "fonts":[{"image":"font/example.png","glyphs":[{"tile":1,"text_code_meaning":"A"}]}]})");
        write("screens/index.json", R"({"schema_version":1,"scenes":[{}]})");
    }

    [[nodiscard]] nlohmann::json read(const std::filesystem::path& relative_path) const
    {
        std::ifstream input(root_ / relative_path);
        input.exceptions(std::ios::failbit | std::ios::badbit);
        nlohmann::json document;
        input >> document;
        return document;
    }

    [[nodiscard]] const std::filesystem::path& root() const noexcept
    {
        return root_;
    }

private:
    std::filesystem::path root_;
};

struct CatalogCollection final {
    const char* path;
    const char* key;
};

} // namespace

TEST_CASE("asset catalog loads every required extractor family")
{
    const TemporaryAssets assets;
    assets.write_catalogs();

    const auto catalog = dw4::content::AssetCatalog::load(assets.root());
    CHECK(catalog.summary().dialogue_messages == 2);
    CHECK(catalog.summary().locations == 1);
    CHECK(catalog.summary().field_sprite_sets == 3);
    CHECK(catalog.summary().monsters == 4);
    CHECK(catalog.summary().music_tracks == 2);
    CHECK(catalog.summary().sound_effects == 3);
    CHECK(catalog.summary().chr_sheets == 2);
    CHECK(catalog.summary().fonts == 1);
    CHECK(catalog.summary().reference_scenes == 1);
}

TEST_CASE("asset catalog rejects incomplete extraction output")
{
    const TemporaryAssets assets;
    CHECK_THROWS_AS(dw4::content::AssetCatalog::load(assets.root()), std::runtime_error);
}

TEST_CASE("asset catalog rejects non-object JSON roots")
{
    const auto relative_path = GENERATE("text/dialogue.json", "maps/index.json", "sprites/field/index.json",
                                        "monsters/index.json", "audio/index.json", "graphics/index.json",
                                        "screens/index.json");
    const auto json = GENERATE("[]", "null", "false", "42", R"("catalog")");
    CAPTURE(relative_path, json);

    const TemporaryAssets assets;
    assets.write_catalogs();
    assets.write(relative_path, json);
    CHECK_THROWS_AS(dw4::content::AssetCatalog::load(assets.root()), std::runtime_error);
}

TEST_CASE("asset catalog rejects missing schema versions with repair instructions")
{
    const auto relative_path = GENERATE("text/dialogue.json", "maps/index.json", "sprites/field/index.json",
                                        "monsters/index.json", "audio/index.json", "graphics/index.json",
                                        "screens/index.json");
    CAPTURE(relative_path);
    const TemporaryAssets assets;
    assets.write_catalogs();
    auto document = assets.read(relative_path);
    document.erase("schema_version");
    assets.write(relative_path, document.dump());

    CHECK_THROWS_WITH(dw4::content::AssetCatalog::load(assets.root()),
                      Catch::Matchers::ContainsSubstring("run extract-native-assets.cmd"));
}

TEST_CASE("asset catalog rejects unsupported and incorrectly typed schema versions")
{
    const auto relative_path = GENERATE("text/dialogue.json", "maps/index.json", "sprites/field/index.json",
                                        "monsters/index.json", "audio/index.json", "graphics/index.json",
                                        "screens/index.json");
    const auto invalid_version = GENERATE("-1", "0", "2", "1.0", "1.5", "true", "null", R"("1")", "[]", "{}");
    CAPTURE(relative_path, invalid_version);
    const TemporaryAssets assets;
    assets.write_catalogs();
    auto document = assets.read(relative_path);
    document["schema_version"] = nlohmann::json::parse(invalid_version);
    assets.write(relative_path, document.dump());

    CHECK_THROWS_WITH(dw4::content::AssetCatalog::load(assets.root()),
                      Catch::Matchers::ContainsSubstring("unsupported asset catalog schema_version"));
}

TEST_CASE("asset catalog requires arrays of object records")
{
    const auto collection = GENERATE(CatalogCollection{"text/dialogue.json", "messages"},
                                     CatalogCollection{"maps/index.json", "locations"},
                                     CatalogCollection{"sprites/field/index.json", "sets"},
                                     CatalogCollection{"monsters/index.json", "monsters"},
                                     CatalogCollection{"audio/index.json", "music"},
                                     CatalogCollection{"audio/index.json", "sfx"},
                                     CatalogCollection{"graphics/index.json", "chr"},
                                     CatalogCollection{"graphics/index.json", "fonts"},
                                     CatalogCollection{"screens/index.json", "scenes"});
    const auto invalid_json = GENERATE("null", "{}", "false", "42", R"("records")", "[null]", "[42]", "[[]]", "[false]");
    CAPTURE(collection.path, collection.key, invalid_json);

    const TemporaryAssets assets;
    assets.write_catalogs();
    auto document = assets.read(collection.path);
    document[collection.key] = nlohmann::json::parse(invalid_json);
    assets.write(collection.path, document.dump());
    CHECK_THROWS_AS(dw4::content::AssetCatalog::load(assets.root()), std::runtime_error);
}

TEST_CASE("asset catalog rejects missing required collections")
{
    const auto collection = GENERATE(CatalogCollection{"text/dialogue.json", "messages"},
                                     CatalogCollection{"maps/index.json", "locations"},
                                     CatalogCollection{"sprites/field/index.json", "sets"},
                                     CatalogCollection{"monsters/index.json", "monsters"},
                                     CatalogCollection{"audio/index.json", "music"},
                                     CatalogCollection{"audio/index.json", "sfx"},
                                     CatalogCollection{"graphics/index.json", "chr"},
                                     CatalogCollection{"graphics/index.json", "fonts"},
                                     CatalogCollection{"screens/index.json", "scenes"});
    CAPTURE(collection.path, collection.key);

    const TemporaryAssets assets;
    assets.write_catalogs();
    auto document = assets.read(collection.path);
    document.erase(collection.key);
    assets.write(collection.path, document.dump());
    CHECK_THROWS_AS(dw4::content::AssetCatalog::load(assets.root()), std::runtime_error);
}

TEST_CASE("asset catalog exposes sorted immutable dialogue records")
{
    const TemporaryAssets assets;
    assets.write_catalogs();
    assets.write("text/dialogue.json", R"({"schema_version":1,"messages":[{"id":32,"group":1,"index":0,"symbols":[0,64,255]},
                                                        {"id":1,"group":0,"index":1,"symbols":[]}]})");

    const auto catalog = dw4::content::AssetCatalog::load(assets.root());
    const auto messages = catalog.dialogue_messages();
    REQUIRE(messages.size() == 2);
    CHECK(messages[0].id == dw4::content::DialogueMessageId{1});
    CHECK(messages[1].id == dw4::content::DialogueMessageId{32});
    const auto& message = catalog.dialogue_message({32});
    REQUIRE(message.symbols.size() == 3);
    CHECK(message.symbols[0] == 0);
    CHECK(message.symbols[1] == 64);
    CHECK(message.symbols[2] == 255);
    CHECK(catalog.dialogue_message({1}).symbols.empty());
    CHECK_THROWS_AS(catalog.dialogue_message({0}), std::out_of_range);
}

TEST_CASE("asset catalog rejects invalid dialogue records")
{
    const auto record = GENERATE(
        R"(null)", R"([])", R"({})",
        R"({"id":-1,"group":0,"index":0,"symbols":[]})",
        R"({"id":1.5,"group":0,"index":0,"symbols":[]})",
        R"({"id":65536,"group":0,"index":0,"symbols":[]})",
        R"({"id":true,"group":0,"index":0,"symbols":[]})",
        R"({"id":1,"group":0,"index":0,"symbols":[]})",
        R"({"id":32,"group":0,"index":32,"symbols":[]})",
        R"({"id":0,"group":-1,"index":0,"symbols":[]})",
        R"({"id":0,"group":2048,"index":0,"symbols":[]})",
        R"({"id":0,"group":0,"index":0})",
        R"({"id":0,"group":0,"index":0,"symbols":null})",
        R"({"id":0,"group":0,"index":0,"symbols":[-1]})",
        R"({"id":0,"group":0,"index":0,"symbols":[256]})",
        R"({"id":0,"group":0,"index":0,"symbols":[1.5]})",
        R"({"id":0,"group":0,"index":0,"symbols":[false]})");
    CAPTURE(record);

    const TemporaryAssets assets;
    assets.write_catalogs();
    assets.write("text/dialogue.json", "{\"schema_version\":1,\"messages\":[" + std::string(record) + "]}");
    CHECK_THROWS_AS(dw4::content::AssetCatalog::load(assets.root()), std::runtime_error);
}

TEST_CASE("asset catalog rejects duplicate dialogue identifiers")
{
    const TemporaryAssets assets;
    assets.write_catalogs();
    assets.write("text/dialogue.json", R"({"schema_version":1,"messages":[{"id":1,"group":0,"index":1,"symbols":[1]},
                                                        {"id":1,"group":0,"index":1,"symbols":[2]}]})");
    CHECK_THROWS_WITH(dw4::content::AssetCatalog::load(assets.root()),
                      Catch::Matchers::ContainsSubstring("duplicate dialogue message id 1"));
}

TEST_CASE("asset catalog reports malformed JSON without exposing payloads")
{
    const TemporaryAssets assets;
    assets.write_catalogs();
    assets.write("text/dialogue.json", R"({"messages": [private-payload]})");
    try {
        static_cast<void>(dw4::content::AssetCatalog::load(assets.root()));
        FAIL("malformed JSON must be rejected");
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        CHECK(message.find("invalid JSON at byte") != std::string::npos);
        CHECK(message.find("dialogue.json") != std::string::npos);
        CHECK(message.find("private-payload") == std::string::npos);
    }
}

TEST_CASE("replay files round trip complete engine state and sparse checkpoints")
{
    const TemporaryAssets files;
    dw4::content::ReplayDocument document;
    document.initial.frame = 71;
    document.initial.random_engine.seed(73);
    document.initial.random_draw_counter = 9;
    document.frames = {{1, 2}, {3, 0}, {0, 1}};
    dw4::game::FrameState initial;
    initial.number = document.initial.frame;
    initial.random = {document.initial.random_engine, document.initial.random_draw_counter};
    dw4::game::Game game(initial);
    dw4::game::Replay replay;
    replay.initial = initial;
    for (std::uint32_t index = 0; index < document.frames.size(); ++index) {
        const auto& frame = document.frames[index];
        game.tick({frame.input});
        for (std::uint32_t draw = 0; draw < frame.random_draws; ++draw) {
            static_cast<void>(game.next_random_byte());
        }
        const auto& state = game.frame();
        document.checkpoints.push_back({index, {state.number, state.input.buttons, state.pressed.buttons, state.released.buttons,
                                                state.random.engine, state.random.draw_counter}});
    }
    const auto path = files.root() / "replay.json";
    dw4::content::ReplayFile::save(path, document);
    const auto loaded = dw4::content::ReplayFile::load(path);
    REQUIRE(loaded.frames.size() == document.frames.size());
    REQUIRE(loaded.checkpoints.size() == document.checkpoints.size());
    CHECK(loaded.initial.random_engine == document.initial.random_engine);
    CHECK(loaded.initial.random_draw_counter == document.initial.random_draw_counter);
    for (const auto& frame : loaded.frames) {
        replay.frames.push_back({{frame.input}, frame.random_draws});
    }
    for (const auto& checkpoint : loaded.checkpoints) {
        const auto& snapshot = checkpoint.snapshot;
        dw4::game::FrameState expected;
        expected.number = snapshot.frame;
        expected.input.buttons = snapshot.input;
        expected.pressed.buttons = snapshot.pressed;
        expected.released.buttons = snapshot.released;
        expected.random = {snapshot.random_engine, snapshot.random_draw_counter};
        replay.checkpoints.push_back({checkpoint.frame_index, expected});
    }
    const auto result = dw4::game::run_replay(replay);
    CHECK_FALSE(result.difference);
    CHECK(result.final_state == game.frame());

    std::array<char, 4096> export_directory{};
    std::size_t export_length = 0;
    const auto environment_result = getenv_s(&export_length, export_directory.data(), export_directory.size(),
                                             "DW4_REPLAY_SMOKE_OUTPUT");
    REQUIRE(environment_result == 0);
    if (export_length > 1) {
        const std::filesystem::path root(export_directory.data());
        std::filesystem::create_directories(root);
        dw4::content::ReplayFile::save(root / "matching.json", document);
        document.checkpoints[1].snapshot.frame += 1;
        dw4::content::ReplayFile::save(root / "divergent.json", document);
        dw4::content::ReplayDocument live;
        live.frames.resize(620);
        live.frames[400].input = static_cast<std::uint8_t>(dw4::game::Button::start);
        for (const auto frame : {430, 450, 470, 510, 530, 550, 570}) {
            live.frames[static_cast<std::size_t>(frame)].input = static_cast<std::uint8_t>(dw4::game::Button::a);
        }
        live.frames[490].input = static_cast<std::uint8_t>(dw4::game::Button::left);
        dw4::content::ReplayFile::save(root / "live-menu.json", live);
    }
}

TEST_CASE("replay files reject incompatible headers and malformed engine state")
{
    const auto mutation = GENERATE(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
    CAPTURE(mutation);
    const TemporaryAssets files;
    const auto path = files.root() / "replay.json";
    dw4::content::ReplayDocument initial;
    initial.frames.push_back({0, 0});
    dw4::content::ReplayFile::save(path, initial);
    auto document = files.read("replay.json");
    switch (mutation) {
    case 0: document["schema_version"] = 2; break;
    case 1: document["rules_version"] = "future"; break;
    case 2: document["rng_compatibility"] = "different-library"; break;
    case 3: document["asset_schema_version"] = 2; break;
    case 4: document["initial"]["random_engine"] = "broken"; break;
    case 5: document["initial"]["random_engine"] = document["initial"]["random_engine"].get<std::string>() + " junk"; break;
    case 6: document["frames"][0]["input"] = 256; break;
    case 7: document["frames"][0]["random_draws"] = -1; break;
    case 8: document["initial"]["frame"] = std::numeric_limits<std::uint64_t>::max(); break;
    case 9: document["initial"]["pressed"] = 1; break;
    case 10: document["checkpoints"] = nlohmann::json::array({{{"frame_index", 1}, {"snapshot", document["initial"]}}}); break;
    default: FAIL("invalid fixture mutation");
    }
    files.write("replay.json", document.dump());
    CHECK_THROWS_AS(dw4::content::ReplayFile::load(path), std::runtime_error);
}

TEST_CASE("replay files validate before truncating existing output")
{
    const TemporaryAssets files;
    files.write("replay.json", R"({"authored":"preserve-this"})");
    const auto before = files.read("replay.json");
    dw4::content::ReplayDocument invalid;
    invalid.frames.push_back({0, dw4::content::replay_file_max_draws + 1U});
    CHECK_THROWS_AS(dw4::content::ReplayFile::save(files.root() / "replay.json", invalid), std::runtime_error);
    CHECK(files.read("replay.json") == before);
}

TEST_CASE("replay files reject missing truncated and trailing documents")
{
    const auto malformed = GENERATE("", "{", "[]", "null", "{\"schema_version\":1} trailing");
    CAPTURE(malformed);
    const TemporaryAssets files;
    files.write("replay.json", malformed);
    CHECK_THROWS_AS(dw4::content::ReplayFile::load(files.root() / "replay.json"), std::runtime_error);
    CHECK_THROWS_AS(dw4::content::ReplayFile::load(files.root() / "missing.json"), std::runtime_error);
}

TEST_CASE("asset graphics expose immutable tile and font records")
{
    const TemporaryAssets assets;
    assets.write_catalogs();
    const auto catalog = dw4::content::AssetCatalog::load(assets.root());
    REQUIRE(catalog.tile_sheets().size() == 2);
    CHECK(catalog.tile_sheets()[0].image == "tiles-a.png");
    CHECK(catalog.tile_sheets()[0].format == dw4::content::TilePixelFormat::indexed_four_color);
    CHECK(catalog.tile_sheets()[0].tile_count == 3);
    CHECK(catalog.tile_sheets()[1].format == dw4::content::TilePixelFormat::monochrome);
    REQUIRE(catalog.font_sheets().size() == 1);
    REQUIRE(catalog.font_sheets()[0].glyphs.size() == 1);
    CHECK(catalog.font_sheets()[0].glyphs[0].tile == 1);
    CHECK(catalog.font_sheets()[0].glyphs[0].meaning == "A");
}

TEST_CASE("asset graphics reject invalid image and format records")
{
    const auto mutation = GENERATE(0, 1, 2, 3, 4, 5, 6, 7, 8);
    CAPTURE(mutation);
    const TemporaryAssets assets;
    assets.write_catalogs();
    auto document = assets.read("graphics/index.json");
    switch (mutation) {
    case 0: document["chr"][0]["image"] = "../outside.png"; break;
    case 1: document["chr"][0]["image"] = "C:/outside.png"; break;
    case 2: document["chr"][0]["image"] = "file.wav"; break;
    case 3: document["chr"][0]["format"] = "8bpp"; break;
    case 4: document["chr"][0]["tiles"] = 0; break;
    case 5: document["chr"][0]["tiles"] = 1.5; break;
    case 6: document["chr"][1]["image"] = "tiles-a.png"; break;
    case 7: document["fonts"][0]["glyphs"].push_back(document["fonts"][0]["glyphs"][0]); break;
    case 8: document["fonts"][0]["glyphs"][0]["tile"] = 65536; break;
    default: FAIL("invalid graphics fixture mutation");
    }
    assets.write("graphics/index.json", document.dump());
    CHECK_THROWS_AS(dw4::content::AssetCatalog::load(assets.root()), std::runtime_error);
}

TEST_CASE("graphics decoder uses SDL PNG decoding and preserves palette indexes")
{
    const TemporaryAssets files;
    files.write_catalogs();
    using Surface = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>;
    const auto write_image = [&](const std::filesystem::path& relative, const bool monochrome) {
        const auto path = files.root() / relative;
        std::filesystem::create_directories(path.parent_path());
        Surface surface(SDL_CreateSurface(128, 8, SDL_PIXELFORMAT_RGBA32), SDL_DestroySurface);
        REQUIRE(surface);
        for (int row = 0; row < 8; ++row) {
            auto* pixels = static_cast<std::uint8_t*>(surface->pixels) + static_cast<std::size_t>(row) * surface->pitch;
            for (int column = 0; column < 128; ++column) {
                const auto shade = static_cast<std::uint8_t>(monochrome ? (column % 2) * 255 : (column % 4) * 85);
                auto* color = pixels + column * 4;
                color[0] = shade;
                color[1] = shade;
                color[2] = shade;
                color[3] = 255;
            }
        }
        REQUIRE(IMG_SavePNG(surface.get(), path.string().c_str()));
    };
    write_image("graphics/chr/tiles-a.png", false);
    write_image("graphics/chr/tiles-b.png", true);
    write_image("graphics/font/example.png", true);
    const auto catalog = dw4::content::AssetCatalog::load(files.root());
    const auto graphics = dw4::desktop::GraphicsAssets::load(catalog);
    REQUIRE(graphics.tiles.size() == 2);
    REQUIRE(graphics.fonts.size() == 1);
    CHECK(graphics.tiles[0].width == 128);
    CHECK(graphics.tiles[0].height == 8);
    for (std::size_t index = 0; index < 128; ++index) {
        CHECK(graphics.tiles[0].pixels[index] == index % 4);
        CHECK(graphics.tiles[1].pixels[index] == index % 2);
    }
    auto document = files.read("graphics/index.json");
    document["chr"][0]["tiles"] = 17;
    files.write("graphics/index.json", document.dump());
    CHECK_THROWS_AS(dw4::desktop::GraphicsAssets::load(dw4::content::AssetCatalog::load(files.root())), std::runtime_error);
}

TEST_CASE("software tile rendering preserves colors flips clipping and draw order")
{
    using Surface = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>;
    using Renderer = std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)>;
    Surface output(SDL_CreateSurface(256, 240, SDL_PIXELFORMAT_RGBA32), SDL_DestroySurface);
    REQUIRE(output);
    Renderer renderer(SDL_CreateSoftwareRenderer(output.get()), SDL_DestroyRenderer);
    REQUIRE(renderer);
    dw4::desktop::GraphicsAssets assets;
    dw4::desktop::IndexedImage image{128, 8, std::vector<std::uint8_t>(128 * 8, 0)};
    image.pixels[0] = 1;
    image.pixels[7] = 2;
    image.pixels[7 * 128] = 3;
    assets.tiles.push_back(std::move(image));
    dw4::desktop::GraphicsRenderer graphics(renderer.get(), assets);
    const dw4::game::Color background{12, 24, 36, 255};
    const dw4::game::Palette palette{{{0, 0, 0, 0}, {240, 20, 40, 255}, {20, 240, 60, 255}, {40, 60, 240, 255}}};
    graphics.begin(background);
    graphics.draw({dw4::game::GraphicKind::tile_sheet, 0, 0, 0, 0, palette});
    graphics.draw({dw4::game::GraphicKind::tile_sheet, 0, 0, 16, 0, palette, true, false});
    graphics.draw({dw4::game::GraphicKind::tile_sheet, 0, 0, 32, 0, palette, false, true});
    graphics.draw({dw4::game::GraphicKind::tile_sheet, 0, 0, 48, 0, palette, true, true});
    graphics.draw({dw4::game::GraphicKind::tile_sheet, 0, 0, -7, 16, palette});
    graphics.draw({dw4::game::GraphicKind::tile_sheet, 0, 0, 255, 239, palette});
    auto alternate = palette;
    alternate[1] = {180, 200, 40, 255};
    graphics.draw({dw4::game::GraphicKind::tile_sheet, 0, 0, 0, 0, alternate});
    Surface pixels(SDL_RenderReadPixels(renderer.get(), nullptr), SDL_DestroySurface);
    REQUIRE(pixels);
    const auto color_at = [&](const int horizontal, const int vertical) {
        dw4::game::Color color;
        REQUIRE(SDL_ReadSurfacePixel(pixels.get(), horizontal, vertical, &color.red, &color.green, &color.blue, &color.alpha));
        return color;
    };
    CHECK(color_at(0, 0) == alternate[1]);
    CHECK(color_at(7, 0) == palette[2]);
    CHECK(color_at(0, 7) == palette[3]);
    CHECK(color_at(1, 1) == background);
    CHECK(color_at(16, 0) == palette[2]);
    CHECK(color_at(23, 0) == palette[1]);
    CHECK(color_at(32, 0) == palette[3]);
    CHECK(color_at(48, 7) == palette[2]);
    CHECK(color_at(55, 7) == palette[1]);
    CHECK(color_at(0, 16) == palette[2]);
    CHECK(color_at(255, 239) == palette[1]);
    CHECK_THROWS_AS(graphics.draw({dw4::game::GraphicKind::tile_sheet, 1, 0, 0, 0, palette}), std::out_of_range);
    CHECK_THROWS_AS(graphics.draw({dw4::game::GraphicKind::tile_sheet, 0, 16, 0, 0, palette}), std::out_of_range);
}

TEST_CASE("software presentation uses integer scaling and letterboxing")
{
    const auto dimensions = GENERATE(std::pair{256, 240}, std::pair{512, 480}, std::pair{768, 720},
                                     std::pair{800, 600}, std::pair{1280, 720}, std::pair{1440, 900}, std::pair{3200, 1080});
    CAPTURE(dimensions.first, dimensions.second);
    using Surface = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>;
    using Renderer = std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)>;
    Surface output(SDL_CreateSurface(dimensions.first, dimensions.second, SDL_PIXELFORMAT_RGBA32), SDL_DestroySurface);
    REQUIRE(output);
    Renderer renderer(SDL_CreateSoftwareRenderer(output.get()), SDL_DestroyRenderer);
    REQUIRE(renderer);
    dw4::desktop::GraphicsAssets assets;
    assets.tiles.push_back({128, 8, std::vector<std::uint8_t>(128 * 8, 1)});
    dw4::desktop::GraphicsRenderer graphics(renderer.get(), assets);
    const dw4::game::Color background{24, 48, 72, 255};
    const dw4::game::Color marker{224, 96, 112, 255};
    const dw4::game::Palette palette{{background, marker, marker, marker}};
    graphics.begin(background);
    graphics.draw({dw4::game::GraphicKind::tile_sheet, 0, 0, 0, 0, palette});
    graphics.draw({dw4::game::GraphicKind::tile_sheet, 0, 0, 248, 232, palette});
    graphics.finish();
    const auto color_at = [&](const int horizontal, const int vertical) {
        dw4::game::Color color;
        REQUIRE(SDL_ReadSurfacePixel(output.get(), horizontal, vertical, &color.red, &color.green, &color.blue, &color.alpha));
        return color;
    };
    const auto scale = std::min(dimensions.first / 256, dimensions.second / 240);
    const auto left = (dimensions.first - 256 * scale) / 2;
    const auto top = (dimensions.second - 240 * scale) / 2;
    CHECK(color_at(left, top) == marker);
    CHECK(color_at(left + 8 * scale - 1, top + 8 * scale - 1) == marker);
    CHECK(color_at(left + 8 * scale, top + 8 * scale) == background);
    CHECK(color_at(left + 256 * scale - 1, top + 240 * scale - 1) == marker);
    if (left > 0) CHECK(color_at(left - 1, top) == dw4::game::Color{0, 0, 0, 255});
    if (top > 0) CHECK(color_at(left, top - 1) == dw4::game::Color{0, 0, 0, 255});
}

TEST_CASE("adventure windows retain parent geometry and draw opaque children in front")
{
    const TemporaryAssets files;
    files.write_catalogs();
    auto metadata = files.read("graphics/index.json");
    metadata["fonts"][0]["glyphs"] = nlohmann::json::array();
    for (int character = 32; character < 127; ++character) {
        metadata["fonts"][0]["glyphs"].push_back({{"tile", character}, {"text_code_meaning", std::string(1, static_cast<char>(character))}});
    }
    files.write("graphics/index.json", metadata.dump());
    const auto catalog = dw4::content::AssetCatalog::load(files.root());
    const dw4::desktop::AdventureView view(catalog);
    dw4::game::AdventureLog menu({}, view.name_characters(), false);
    const auto root = view.commands(menu);
    REQUIRE(root.size() > 96);
    CHECK(root[0].tile == 134);
    CHECK(root[0].x == 16);
    CHECK(root[0].y == 48);
    CHECK(root[95].x == 200);
    CHECK(root[95].y == 72);
    menu.tick({.confirm = true});
    const auto slot = view.commands(menu);
    REQUIRE(slot.size() > root.size());
    CHECK(slot[root.size()].tile == 134);
    CHECK(slot[root.size()].x == 80);
    CHECK(slot[root.size()].y == 112);
    menu.tick({.confirm = true});
    const auto name = view.commands(menu);
    const auto find_window = [&](const std::vector<dw4::game::TileDraw>& draws, const int x, const int y) {
        return std::ranges::find_if(draws, [&](const auto& draw) { return draw.tile == 134 && draw.x == x && draw.y == y; });
    };
    REQUIRE(find_window(name, 16, 48) != name.end());
    REQUIRE(find_window(name, 80, 112) != name.end());
    REQUIRE(find_window(name, 80, 32) != name.end());
    REQUIRE(find_window(name, 32, 64) != name.end());
    CHECK(find_window(name, 32, 64) > find_window(name, 80, 112));
    for (const auto& draw : name) {
        CHECK(draw.x >= 0); CHECK(draw.x <= 248);
        CHECK(draw.y >= 0); CHECK(draw.y <= 232);
    }
    using Surface = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>;
    using Renderer = std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)>;
    Surface output(SDL_CreateSurface(256, 240, SDL_PIXELFORMAT_RGBA32), SDL_DestroySurface);
    REQUIRE(output);
    Renderer renderer(SDL_CreateSoftwareRenderer(output.get()), SDL_DestroyRenderer);
    REQUIRE(renderer);
    dw4::desktop::GraphicsAssets assets;
    dw4::desktop::IndexedImage font{128, 72, std::vector<std::uint8_t>(128 * 72, 1)};
    for (int row = 0; row < 8; ++row) {
        std::fill_n(font.pixels.begin() + row * 128, 8, std::uint8_t{0});
    }
    assets.fonts.push_back(std::move(font));
    dw4::desktop::GraphicsRenderer graphics(renderer.get(), assets);
    view.draw(graphics, menu);
    Surface pixels(SDL_RenderReadPixels(renderer.get(), nullptr), SDL_DestroySurface);
    REQUIRE(pixels);
    const auto color_at = [&](const int x, const int y) {
        dw4::game::Color color;
        REQUIRE(SDL_ReadSurfacePixel(pixels.get(), x, y, &color.red, &color.green, &color.blue, &color.alpha));
        return color;
    };
    CHECK(color_at(16, 48) == dw4::game::Color{252, 252, 252, 255});
    CHECK(color_at(88, 112) == dw4::game::Color{0, 0, 0, 255});
    CHECK(color_at(32, 64) == dw4::game::Color{252, 252, 252, 255});
    CHECK(color_at(8, 8) == dw4::game::Color{0, 0, 0, 255});
    menu.tick({.confirm = true});
    menu.tick({.left = true}); menu.tick({.confirm = true});
    CHECK(menu.state().page == dw4::game::LogPage::gender);
    const auto gender = view.commands(menu);
    REQUIRE(find_window(gender, 144, 96) != gender.end());
    menu.tick({.confirm = true});
    CHECK(menu.state().page == dw4::game::LogPage::speed);
    const auto speed = view.commands(menu);
    REQUIRE(find_window(speed, 64, 112) != speed.end());
    REQUIRE(find_window(speed, 144, 96) != speed.end());
    CHECK(find_window(speed, 64, 112) > find_window(speed, 144, 96));
    CHECK(std::ranges::any_of(speed, [](const auto& draw) { return draw.tile == 'F' && draw.x == 72 && draw.y == 192; }));
    CHECK(std::ranges::any_of(speed, [](const auto& draw) { return draw.tile == 'S' && draw.x == 168 && draw.y == 192; }));
    CHECK(std::ranges::any_of(speed, [](const auto& draw) { return draw.tile == 'M' && draw.x == 96 && draw.y == 208; }));
    CHECK(std::ranges::any_of(speed, [](const auto& draw) { return draw.tile == 139 && draw.x == 216 && draw.y == 216; }));
    menu.tick({.confirm = true});
    const auto confirmation = view.commands(menu);
    REQUIRE(find_window(confirmation, 192, 48) != confirmation.end());
    CHECK(find_window(confirmation, 192, 48) > find_window(confirmation, 64, 112));
    menu.tick({.cancel = true});
    CHECK(menu.state().page == dw4::game::LogPage::speed);
    menu.tick({.cancel = true});
    CHECK(menu.state().page == dw4::game::LogPage::gender);
    menu.tick({.cancel = true});
    CHECK(menu.state().page == dw4::game::LogPage::name);
    CHECK(menu.state().name.size() == 1);
}

TEST_CASE("title data loads typed placements palettes and bounded animation ranges")
{
    const TemporaryAssets files;
    files.write("title/index.json", R"({"schema_version":1,"image":"patterns.png","tiles":1,"artwork_frame":1,
        "fade_frames":1,"loop_start":0,"loop_frames":1,"palettes":[[[0,0,0,0],[255,255,255,255],[0,0,0,255],[0,0,0,255]]],
        "frames":[{"background":[0,0,0,255],"draws":[[0,-7,0,0,0,0,240]]}],
        "exit_frames":[{"background":[0,0,0,255],"draws":[]}]})");
    const auto data = dw4::content::TitleData::load(files.root() / "title");
    REQUIRE(data.frames.size() == 1);
    REQUIRE(data.frames[0].draws.size() == 1);
    CHECK(data.frames[0].draws[0].x == -7);
    CHECK(data.palettes[0][1][0] == 255);
    CHECK(data.fade_frames == 1);
    auto document = files.read("title/index.json");
    document["frames"][0]["draws"][0][0] = 1;
    files.write("title/index.json", document.dump());
    CHECK_THROWS_AS(dw4::content::TitleData::load(files.root() / "title"), std::runtime_error);
}

TEST_CASE("title data rejects invalid clips timing palette references and music")
{
    const auto mutation = GENERATE(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
    CAPTURE(mutation);
    const TemporaryAssets files;
    files.write("title/index.json", R"({"schema_version":1,"image":"patterns.png","tiles":1,"artwork_frame":1,
        "fade_frames":1,"loop_start":0,"loop_frames":1,"palettes":[[[0,0,0,0],[255,255,255,255],[0,0,0,255],[0,0,0,255]]],
        "frames":[{"background":[0,0,0,255],"draws":[[0,0,0,0,0,0,240]]}],
        "exit_frames":[{"background":[0,0,0,255],"draws":[]}],"music":[{"frame":1,"track":2}]})");
    auto document = files.read("title/index.json");
    switch (mutation) {
    case 0: document["frames"][0]["draws"][0][1] = -8; break;
    case 1: document["frames"][0]["draws"][0][3] = 1; break;
    case 2: document["frames"][0]["draws"][0][4] = 4; break;
    case 3: document["frames"][0]["draws"][0][5] = 239; break;
    case 4: document["frames"][0]["draws"][0][6] = 0; break;
    case 5: document["loop_frames"] = 2; break;
    case 6: document["fade_frames"] = 2; break;
    case 7: document["tiles"] = 0; break;
    case 8: document["music"][0]["track"] = 0; break;
    case 9: document["music"].push_back(document["music"][0]); break;
    case 10: document["image"] = "../outside.png"; break;
    default: FAIL("invalid fixture mutation");
    }
    files.write("title/index.json", document.dump());
    CHECK_THROWS_AS(dw4::content::TitleData::load(files.root() / "title"), std::runtime_error);
}

TEST_CASE("title data resolves shared draw records and rejects unknown references")
{
    const TemporaryAssets files;
    files.write("title/index.json", R"({"schema_version":1,"image":"patterns.png","tiles":1,"artwork_frame":1,
        "fade_frames":1,"loop_start":0,"loop_frames":1,"palettes":[[[0,0,0,0],[255,255,255,255],[0,0,0,255],[0,0,0,255]]],
        "draw_records":[[0,-7,0,0,0,0,240]],"frames":[{"background":[0,0,0,255],"draws":[0]}],
        "exit_frames":[{"background":[0,0,0,255],"draws":[]}]})");
    const auto data = dw4::content::TitleData::load(files.root() / "title");
    REQUIRE(data.frames[0].draws.size() == 1);
    CHECK(data.frames[0].draws[0].x == -7);
    auto document = files.read("title/index.json");
    document["frames"][0]["draws"][0] = 1;
    files.write("title/index.json", document.dump());
    CHECK_THROWS_AS(dw4::content::TitleData::load(files.root() / "title"), std::runtime_error);
}

TEST_CASE("device input normalization maps logical buttons and bounded stick thresholds")
{
    dw4::desktop::DeviceButtons buttons;
    CHECK(dw4::desktop::normalize_buttons(buttons).buttons == 0);
    buttons.a = true;
    buttons.start = true;
    auto frame = dw4::desktop::normalize_buttons(buttons);
    CHECK(frame.is_pressed(dw4::game::Button::a));
    CHECK(frame.is_pressed(dw4::game::Button::start));
    buttons.horizontal = 15999;
    CHECK_FALSE(dw4::desktop::normalize_buttons(buttons).is_pressed(dw4::game::Button::right));
    buttons.horizontal = 16000;
    CHECK(dw4::desktop::normalize_buttons(buttons).is_pressed(dw4::game::Button::right));
    buttons.horizontal = -16000;
    CHECK(dw4::desktop::normalize_buttons(buttons).is_pressed(dw4::game::Button::left));
    buttons.vertical = -16000;
    CHECK(dw4::desktop::normalize_buttons(buttons).is_pressed(dw4::game::Button::up));
    buttons.vertical = 16000;
    CHECK(dw4::desktop::normalize_buttons(buttons).is_pressed(dw4::game::Button::down));
    buttons.b = true;
    buttons.select = true;
    frame = dw4::desktop::normalize_buttons(buttons);
    CHECK(frame.is_pressed(dw4::game::Button::b));
    CHECK(frame.is_pressed(dw4::game::Button::select));
}

TEST_CASE("SDL virtual gamepad input handles polling focus loss and disconnect")
{
    REQUIRE(SDL_InitSubSystem(SDL_INIT_GAMEPAD));
    struct Shutdown final { ~Shutdown() { SDL_QuitSubSystem(SDL_INIT_GAMEPAD); } } shutdown;
    SDL_VirtualJoystickDesc description;
    SDL_INIT_INTERFACE(&description);
    description.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    description.naxes = SDL_GAMEPAD_AXIS_COUNT;
    description.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    description.axis_mask = (1U << SDL_GAMEPAD_AXIS_COUNT) - 1U;
    description.button_mask = (1U << SDL_GAMEPAD_BUTTON_COUNT) - 1U;
    description.name = "DW4 authored virtual gamepad";
    const auto identifier = SDL_AttachVirtualJoystick(&description);
    REQUIRE(identifier != 0);
    struct Detach final { SDL_JoystickID id; ~Detach() { if (SDL_IsJoystickVirtual(id)) SDL_DetachVirtualJoystick(id); } } detach{identifier};
    using Joystick = std::unique_ptr<SDL_Joystick, decltype(&SDL_CloseJoystick)>;
    Joystick joystick(SDL_OpenJoystick(identifier), SDL_CloseJoystick);
    REQUIRE(joystick);
    dw4::desktop::Input input(identifier);
    REQUIRE(SDL_SetJoystickVirtualButton(joystick.get(), SDL_GAMEPAD_BUTTON_SOUTH, true));
    REQUIRE(SDL_SetJoystickVirtualButton(joystick.get(), SDL_GAMEPAD_BUTTON_START, true));
    REQUIRE(SDL_SetJoystickVirtualAxis(joystick.get(), SDL_GAMEPAD_AXIS_LEFTX, 20000));
    SDL_UpdateGamepads();
    auto sample = input.sample();
    CHECK(sample.is_pressed(dw4::game::Button::a));
    CHECK(sample.is_pressed(dw4::game::Button::start));
    CHECK(sample.is_pressed(dw4::game::Button::right));
    SDL_Event event{};
    event.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    input.handle(event);
    CHECK(input.sample().buttons == 0);
    event.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
    input.handle(event);
    CHECK(input.sample().buttons == 0);
    REQUIRE(SDL_SetJoystickVirtualButton(joystick.get(), SDL_GAMEPAD_BUTTON_SOUTH, false));
    REQUIRE(SDL_SetJoystickVirtualButton(joystick.get(), SDL_GAMEPAD_BUTTON_START, false));
    REQUIRE(SDL_SetJoystickVirtualAxis(joystick.get(), SDL_GAMEPAD_AXIS_LEFTX, 0));
    SDL_UpdateGamepads();
    CHECK(input.sample().buttons == 0);
    REQUIRE(SDL_SetJoystickVirtualButton(joystick.get(), SDL_GAMEPAD_BUTTON_SOUTH, true));
    SDL_UpdateGamepads();
    CHECK(input.sample().is_pressed(dw4::game::Button::a));
    REQUIRE(SDL_DetachVirtualJoystick(identifier));
    event.type = SDL_EVENT_GAMEPAD_REMOVED;
    event.gdevice.which = identifier;
    input.handle(event);
    CHECK_FALSE(input.sample().is_pressed(dw4::game::Button::start));
}

TEST_CASE("adventure saves round trip name profile and complete native random state")
{
    const TemporaryAssets files;
    dw4::content::AdventureSave save;
    save.name = {37, 38, 39, 40, 41, 42, 43, 44};
    save.gender = 1;
    save.message_speed = 7;
    save.frame = 1234;
    save.random_engine.seed(73);
    static_cast<void>(save.random_engine());
    save.random_draws = 1;
    const auto path = files.root() / "slot.json";
    dw4::content::AdventureSaveFile::save(path, save);
    CHECK(dw4::content::AdventureSaveFile::load(path) == save);
    auto document = files.read("slot.json");
    document["version"] = 2;
    files.write("slot.json", document.dump());
    CHECK_THROWS_AS(dw4::content::AdventureSaveFile::load(path), std::runtime_error);
    dw4::content::AdventureSaveFile::save(path, save);
    document = files.read("slot.json");
    document["message_speed"] = 1;
    files.write("slot.json", document.dump());
    CHECK_THROWS_AS(dw4::content::AdventureSaveFile::load(path), std::runtime_error);
    save.name.clear();
    CHECK_THROWS_AS(dw4::content::AdventureSaveFile::save(path, save), std::runtime_error);
}

TEST_CASE("adventure store performs transactional slot creation copy erase and restart")
{
    const TemporaryAssets files;
    dw4::desktop::AdventureStore store(files.root());
    CHECK_FALSE(store.inspect()[0].save);
    dw4::content::AdventureSave save;
    save.name = {37, 38};
    store.write(0, save);
    CHECK(store.inspect()[0].save == save);
    store.copy(0, 1);
    CHECK(store.inspect()[1].save == save);
    CHECK_THROWS_AS(store.copy(0, 0), std::invalid_argument);
    auto invalid = save;
    invalid.name.clear();
    CHECK_THROWS_AS(store.write(0, invalid), std::runtime_error);
    CHECK(store.inspect()[0].save == save);
    auto changed = save;
    changed.name = {39};
    store.write(0, changed);
    CHECK(store.inspect()[0].save == changed);
    CHECK(dw4::content::AdventureSaveFile::load(files.root() / "adventure-1.json.backup") == save);
    store.erase(1);
    CHECK_FALSE(store.inspect()[1].save);
    dw4::desktop::AdventureStore restarted(files.root());
    CHECK(restarted.inspect()[0].save == changed);
    files.write("adventure-3.json", "broken-save");
    CHECK_FALSE(restarted.inspect()[2].error.empty());
    CHECK_THROWS_AS(store.write(3, save), std::out_of_range);
}

TEST_CASE("adventure gameplay saves preserve roster actors story and resume state")
{
    const TemporaryAssets files;
    dw4::content::AdventureSave save;
    save.name = {37};
    save.world.emplace();
    auto& world = *save.world;
    world.map = 2; world.submap = 1; world.x = 16; world.y = 21; world.gold = 50;
    for (std::uint8_t id = 0; id < world.characters.size(); ++id) world.characters[id].id = id;
    world.characters[6].hp = world.characters[6].maximum_hp = 27;
    world.characters[6].inventory = {{2, 1}, {38, 2}};
    world.story_flags = {1, 32};
    world.actors = {{17, 24, 14, 21, 15, 21, 1, 8, 2, 3, true}};
    world.event_step = 4; world.event_delay = 10;
    const auto path = files.root() / "gameplay.json";
    dw4::content::AdventureSaveFile::save(path, save);
    CHECK(files.read("gameplay.json")["version"] == 2);
    CHECK(dw4::content::AdventureSaveFile::load(path) == save);
    const auto good = save;
    world.party[1] = 6;
    CHECK_THROWS_AS(dw4::content::AdventureSaveFile::save(path, save), std::runtime_error);
    save = good; save.world->characters[6].hp = 28;
    CHECK_THROWS_AS(dw4::content::AdventureSaveFile::save(path, save), std::runtime_error);
    save = good; save.world->characters[6].inventory.resize(9);
    CHECK_THROWS_AS(dw4::content::AdventureSaveFile::save(path, save), std::runtime_error);
    save = good; save.world->story_flags.push_back(1);
    CHECK_THROWS_AS(dw4::content::AdventureSaveFile::save(path, save), std::runtime_error);
    save = good; save.world->actors.push_back(save.world->actors[0]);
    CHECK_THROWS_AS(dw4::content::AdventureSaveFile::save(path, save), std::runtime_error);
    save = good; save.world->x = -1;
    CHECK_THROWS_AS(dw4::content::AdventureSaveFile::save(path, save), std::runtime_error);
    CHECK(dw4::content::AdventureSaveFile::load(path) == good);
}

TEST_CASE("field data validates grid shape actor identity and cross map destinations")
{
    const TemporaryAssets files;
    const nlohmann::json palette = {{0, 0, 0, 255}, {255, 255, 255, 255}, {255, 0, 0, 255}, {0, 255, 0, 255}};
    const nlohmann::json frame{{"quadrants", {0, 0, 0, 0}}, {"palette", 0}, {"flip_h", {false, false, false, false}}};
    const nlohmann::json tile{{"quadrants", {0, 0, 0, 0}}, {"palette", 0}, {"behavior", 0}};
    auto document = nlohmann::json{{"schema_version", 1}, {"image", "patterns.png"}, {"tile_count", 1},
        {"sprite_palettes", nlohmann::json::array({palette, palette, palette, palette})},
        {"sprites", nlohmann::json::array({{{"id", 6}, {"frames", std::vector<nlohmann::json>(8, frame)}}})},
        {"maps", nlohmann::json::array({{{"map", 2}, {"submap", 1}, {"width", 2}, {"height", 2}, {"border", 0}, {"cells", {0, 0, 0, 0}},
            {"tiles", std::vector<nlohmann::json>(32, tile)}, {"palettes", nlohmann::json::array({palette, palette, palette, palette})},
            {"actors", nlohmann::json::array({{{"id", 1}, {"sprite", 6}, {"message", 769}, {"x", 1}, {"y", 1}, {"facing", 2}, {"motion", 0},
                {"script", {{"bytes", nlohmann::json::array()}}}}})}}})}};
    files.write("field/index.json", document.dump());
    const auto data = dw4::content::FieldData::load(files.root() / "field");
    CHECK(data.map(2, 1).cells.size() == 4);
    CHECK(data.map(2, 1).actors[0].message == 769);
    CHECK(data.sprite(6).frames.size() == 8);
    CHECK_THROWS_AS(data.map(3, 0), std::out_of_range);
    document["maps"][0]["cells"].push_back(0);
    files.write("field/index.json", document.dump());
    CHECK_THROWS_AS(dw4::content::FieldData::load(files.root() / "field"), std::runtime_error);
    document["maps"][0]["cells"].erase(4);
    document["maps"][0]["actors"][0]["sprite"] = 9;
    files.write("field/index.json", document.dump());
    CHECK_THROWS_AS(dw4::content::FieldData::load(files.root() / "field"), std::runtime_error);
}