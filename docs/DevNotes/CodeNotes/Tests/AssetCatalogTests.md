# AssetCatalogTests.cpp

**Source:** `tests\src\integration\AssetCatalogTests.cpp`

```cpp
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
```

## Test Dependencies and Isolation

```cpp
#include <dw4/content/AssetCatalog.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

namespace {
```

- The test depends on the public content API and Catch2, then uses standard-library facilities to create fixture files.
- The anonymous namespace keeps `TemporaryAssets` local to this test translation unit.
- The fixtures are small authored JSON strings, not ROM-derived output. They can be committed safely and make failures
    easy to understand.

## Temporary Directory RAII Fixture

```cpp
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
```

- The constructor chooses the operating system's temporary directory and appends a timestamp-derived unique-looking
    name. `steady_clock` is sufficient for ordinary isolated test runs but is not a formal collision guarantee across
    processes; a stronger unique-ID strategy may be appropriate if tests run heavily in parallel.
- The member initializer list constructs `root_` before the constructor body creates the directory.
- The destructor uses the `std::error_code` overload of `remove_all`, which reports failure without throwing. A test
    fixture destructor must not replace an active test failure with a cleanup exception.
- Copying is deleted because two fixtures owning the same directory would both attempt cleanup. The class also declares
    no move operations, so instances remain tied to their original scope.

## Writing Authored Fixture Files

```cpp
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
```

- `write` accepts a path relative to the fixture root and a non-owning view of JSON text.
- `create_directories` creates every missing parent component, allowing the test to reproduce the extractor hierarchy.
- `std::ofstream` owns the file handle and closes it automatically when `write` returns.
- The method is `const` because writing files beneath the path does not change the stored `root_` value.
- The baseline helper does not check stream-open or write errors explicitly. A failed write is expected to make catalog
    loading fail, but direct fixture diagnostics would be clearer in a more mature test utility.
- `root()` returns a borrowed const reference; the fixture must outlive any use of that reference.

## Complete-Family Success Case

```cpp
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
```

- Raw string literals `R"(...)"` allow JSON quotes without C++ backslash escaping.
- Empty objects are sufficient because the baseline validates collection presence and size, not element schemas.
- Different array lengths make each assertion prove that the correct file and key populated the correct summary member.
- Empty graphics and screen objects prove those required documents are opened and parsed even though no counts are read.
- `TemporaryAssets` is destroyed at the test case's closing brace, recursively deleting all fixture files.

## Incomplete-Family Failure Case

```cpp
TEST_CASE("asset catalog rejects incomplete extraction output")
{
        const TemporaryAssets assets;
        CHECK_THROWS_AS(dw4::content::AssetCatalog::load(assets.root()), std::runtime_error);
}
```

- The fixture root exists but contains none of the required files.
- Loading fails on the first required document, `text/dialogue.json`.
- `CHECK_THROWS_AS` verifies that the operation throws `std::runtime_error` or a compatible derived type.
- This proves incomplete extraction does not silently create an empty catalog.
- It does not yet test malformed JSON, a missing field, a field with the wrong type, failures in later families, path
    normalization, or the exact diagnostic text. Those are appropriate additions as the content contract grows.
