#include <dw4/content/AssetCatalog.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

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

    [[nodiscard]] const std::filesystem::path& root() const noexcept
    {
        return root_;
    }

private:
    std::filesystem::path root_;
};

} // namespace

TEST_CASE("asset catalog loads every required extractor family")
{
    const TemporaryAssets assets;
    assets.write("text/dialogue.json", R"({"messages":[{},{}]})");
    assets.write("maps/index.json", R"({"locations":[{}]})");
    assets.write("sprites/field/index.json", R"({"sets":[{},{},{}]})");
    assets.write("monsters/index.json", R"({"monsters":[{},{},{},{}]})");
    assets.write("audio/index.json", R"({"music":[{},{}],"sfx":[{},{},{}]})");
    assets.write("graphics/index.json", R"({})");
    assets.write("screens/index.json", R"({})");

    const auto catalog = dw4::content::AssetCatalog::load(assets.root());
    CHECK(catalog.summary().dialogue_messages == 2);
    CHECK(catalog.summary().locations == 1);
    CHECK(catalog.summary().field_sprite_sets == 3);
    CHECK(catalog.summary().monsters == 4);
    CHECK(catalog.summary().music_tracks == 2);
    CHECK(catalog.summary().sound_effects == 3);
}

TEST_CASE("asset catalog rejects incomplete extraction output")
{
    const TemporaryAssets assets;
    CHECK_THROWS_AS(dw4::content::AssetCatalog::load(assets.root()), std::runtime_error);
}