#include <dw4/content/AssetCatalog.hpp>

#include <fstream>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

namespace dw4::content {
namespace {

using Json = nlohmann::json;

Json read_json(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("required asset catalog is missing: " + path.string());
    }

    Json document;
    input >> document;
    return document;
}

std::size_t array_size(const Json& document, const char* key, const std::filesystem::path& path)
{
    const auto entry = document.find(key);
    if (entry == document.end() || !entry->is_array()) {
        throw std::runtime_error("asset catalog field '" + std::string(key) + "' is not an array: " + path.string());
    }
    return entry->size();
}

} // namespace

AssetCatalog AssetCatalog::load(const std::filesystem::path& root)
{
    AssetCatalog catalog;
    catalog.root_ = std::filesystem::absolute(root).lexically_normal();

    const auto dialogue_path = catalog.root_ / "text" / "dialogue.json";
    const auto maps_path = catalog.root_ / "maps" / "index.json";
    const auto sprites_path = catalog.root_ / "sprites" / "field" / "index.json";
    const auto monsters_path = catalog.root_ / "monsters" / "index.json";
    const auto audio_path = catalog.root_ / "audio" / "index.json";

    catalog.summary_.dialogue_messages = array_size(read_json(dialogue_path), "messages", dialogue_path);
    catalog.summary_.locations = array_size(read_json(maps_path), "locations", maps_path);
    catalog.summary_.field_sprite_sets = array_size(read_json(sprites_path), "sets", sprites_path);
    catalog.summary_.monsters = array_size(read_json(monsters_path), "monsters", monsters_path);

    const Json audio = read_json(audio_path);
    catalog.summary_.music_tracks = array_size(audio, "music", audio_path);
    catalog.summary_.sound_effects = array_size(audio, "sfx", audio_path);

    static_cast<void>(read_json(catalog.root_ / "graphics" / "index.json"));
    static_cast<void>(read_json(catalog.root_ / "screens" / "index.json"));
    return catalog;
}

const std::filesystem::path& AssetCatalog::root() const noexcept
{
    return root_;
}

const AssetSummary& AssetCatalog::summary() const noexcept
{
    return summary_;
}

} // namespace dw4::content