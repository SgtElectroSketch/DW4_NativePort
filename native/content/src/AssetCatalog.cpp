#include <dw4/content/AssetCatalog.hpp>

#include "JsonValidation.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

namespace dw4::content {
namespace {

using Json = nlohmann::json;
using detail::unsigned_field;
using detail::unsigned_value;
constexpr std::uint64_t dialogue_messages_per_group = 32;

Json read_json(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("required asset catalog is missing: " + path.string());
    }

    Json document;
    try {
        input >> document;
    } catch (const Json::parse_error& error) {
        throw std::runtime_error("invalid JSON at byte " + std::to_string(error.byte) +
                                 " in asset catalog: " + path.string());
    }
    if (!document.is_object()) {
        throw std::runtime_error("asset catalog must be a JSON object: " + path.string());
    }
    const auto version = document.find("schema_version");
    if (version == document.end()) {
        throw std::runtime_error("asset catalog schema_version is missing; run extract-native-assets.cmd: " + path.string());
    }
    if (!version->is_number_integer() || *version != asset_catalog_schema_version) {
        throw std::runtime_error("unsupported asset catalog schema_version; expected integer " +
                                 std::to_string(asset_catalog_schema_version) +
                                 "; run extract-native-assets.cmd: " + path.string());
    }
    return document;
}

const Json& record_array(const Json& document, const char* key, const std::filesystem::path& path)
{
    const auto entry = document.find(key);
    if (entry == document.end() || !entry->is_array()) {
        throw std::runtime_error("asset catalog field '" + std::string(key) + "' is not an array: " + path.string());
    }
    for (std::size_t index = 0; index < entry->size(); ++index) {
        if (!(*entry)[index].is_object()) {
            throw std::runtime_error("asset catalog field '" + std::string(key) + "[" + std::to_string(index) +
                                     "]' must be an object: " + path.string());
        }
    }
    return *entry;
}

std::vector<DialogueMessage> read_dialogue(const Json& document, const std::filesystem::path& path)
{
    const auto& records = record_array(document, "messages", path);
    std::vector<DialogueMessage> messages;
    messages.reserve(records.size());
    for (std::size_t record_index = 0; record_index < records.size(); ++record_index) {
        const auto& record = records[record_index];
        const auto field = "messages[" + std::to_string(record_index) + "]";
        const auto id = unsigned_field(record, "id", std::numeric_limits<std::uint16_t>::max(), field + ".id", path);
        const auto group = unsigned_field(record, "group", std::numeric_limits<std::uint16_t>::max() / dialogue_messages_per_group,
                                          field + ".group", path);
        const auto index = unsigned_field(record, "index", dialogue_messages_per_group - 1, field + ".index", path);
        if (id != group * dialogue_messages_per_group + index) {
            throw std::runtime_error("asset catalog field '" + field + ".id' disagrees with group/index: " + path.string());
        }
        const auto symbols = record.find("symbols");
        if (symbols == record.end() || !symbols->is_array()) {
            throw std::runtime_error("asset catalog field '" + field + ".symbols' must be an array: " + path.string());
        }
        DialogueMessage message{{static_cast<std::uint16_t>(id)}, {}};
        message.symbols.reserve(symbols->size());
        for (std::size_t symbol_index = 0; symbol_index < symbols->size(); ++symbol_index) {
            const auto symbol = unsigned_value((*symbols)[symbol_index], std::numeric_limits<std::uint8_t>::max(),
                                               field + ".symbols[" + std::to_string(symbol_index) + "]", path);
            message.symbols.push_back(static_cast<std::uint8_t>(symbol));
        }
        messages.push_back(std::move(message));
    }
    std::ranges::sort(messages, {}, [](const DialogueMessage& message) { return message.id.value; });
    const auto duplicate = std::ranges::adjacent_find(messages, {}, [](const DialogueMessage& message) {
        return message.id.value;
    });
    if (duplicate != messages.end()) {
        throw std::runtime_error("duplicate dialogue message id " + std::to_string(duplicate->id.value) +
                                 " in asset catalog: " + path.string());
    }
    return messages;
}

std::string string_field(const Json& record, const char* key, const std::string& field,
                         const std::filesystem::path& path)
{
    const auto entry = record.find(key);
    if (entry == record.end() || !entry->is_string() || entry->get_ref<const std::string&>().empty()) {
        throw std::runtime_error("asset field '" + field + "' must be a nonempty string: " + path.string());
    }
    return entry->get<std::string>();
}

std::filesystem::path image_field(const Json& record, const std::string& field, const std::filesystem::path& path)
{
    const std::filesystem::path image(string_field(record, "image", field + ".image", path));
    if (image.has_root_path() || image.filename().empty() || image.extension() != ".png") {
        throw std::runtime_error("asset image must be a relative PNG path: " + field + ": " + path.string());
    }
    for (const auto& component : image) {
        if (component == "..") {
            throw std::runtime_error("asset image path must not escape its directory: " + field + ": " + path.string());
        }
    }
    return image.lexically_normal();
}

void read_graphics(const Json& document, const std::filesystem::path& path,
                   std::vector<TileSheet>& sheets, std::vector<FontSheet>& fonts)
{
    const auto& chr_records = record_array(document, "chr", path);
    std::set<std::filesystem::path> images;
    for (std::size_t index = 0; index < chr_records.size(); ++index) {
        const auto& record = chr_records[index];
        const auto field = "chr[" + std::to_string(index) + "]";
        auto image = image_field(record, field, path);
        if (!images.insert(image).second) {
            throw std::runtime_error("duplicate tile sheet image: " + path.string());
        }
        const auto format = string_field(record, "format", field + ".format", path);
        if (format != "1bpp" && format != "2bpp") {
            throw std::runtime_error("unsupported tile sheet format: " + field + ": " + path.string());
        }
        const auto count = unsigned_field(record, "tiles", 65535, field + ".tiles", path);
        if (count == 0) {
            throw std::runtime_error("tile sheet must contain at least one tile: " + field + ": " + path.string());
        }
        sheets.push_back({std::move(image), format == "1bpp" ? TilePixelFormat::monochrome : TilePixelFormat::indexed_four_color,
                          static_cast<std::uint32_t>(count)});
    }
    const auto& font_records = record_array(document, "fonts", path);
    images.clear();
    for (std::size_t index = 0; index < font_records.size(); ++index) {
        const auto& record = font_records[index];
        const auto field = "fonts[" + std::to_string(index) + "]";
        FontSheet font{image_field(record, field, path), {}};
        if (!images.insert(font.image).second) {
            throw std::runtime_error("duplicate font sheet image: " + path.string());
        }
        const auto& glyphs = record_array(record, "glyphs", path);
        std::set<std::uint16_t> tiles;
        for (std::size_t glyph_index = 0; glyph_index < glyphs.size(); ++glyph_index) {
            const auto glyph_field = field + ".glyphs[" + std::to_string(glyph_index) + "]";
            const auto tile = static_cast<std::uint16_t>(unsigned_field(glyphs[glyph_index], "tile", 65535, glyph_field + ".tile", path));
            if (!tiles.insert(tile).second) {
                throw std::runtime_error("duplicate font glyph tile: " + glyph_field + ": " + path.string());
            }
            font.glyphs.push_back({tile, string_field(glyphs[glyph_index], "text_code_meaning", glyph_field + ".text_code_meaning", path)});
        }
        fonts.push_back(std::move(font));
    }
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
    const auto graphics_path = catalog.root_ / "graphics" / "index.json";
    const auto screens_path = catalog.root_ / "screens" / "index.json";

    catalog.dialogue_messages_ = read_dialogue(read_json(dialogue_path), dialogue_path);
    catalog.summary_.dialogue_messages = catalog.dialogue_messages_.size();
    catalog.summary_.locations = record_array(read_json(maps_path), "locations", maps_path).size();
    catalog.summary_.field_sprite_sets = record_array(read_json(sprites_path), "sets", sprites_path).size();
    catalog.summary_.monsters = record_array(read_json(monsters_path), "monsters", monsters_path).size();

    const Json audio = read_json(audio_path);
    catalog.summary_.music_tracks = record_array(audio, "music", audio_path).size();
    catalog.summary_.sound_effects = record_array(audio, "sfx", audio_path).size();

    const Json graphics = read_json(graphics_path);
    read_graphics(graphics, graphics_path, catalog.tile_sheets_, catalog.font_sheets_);
    catalog.summary_.chr_sheets = catalog.tile_sheets_.size();
    catalog.summary_.fonts = catalog.font_sheets_.size();
    catalog.summary_.reference_scenes = record_array(read_json(screens_path), "scenes", screens_path).size();
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

std::span<const DialogueMessage> AssetCatalog::dialogue_messages() const noexcept
{
    return dialogue_messages_;
}

const DialogueMessage& AssetCatalog::dialogue_message(const DialogueMessageId id) const
{
    const auto message = std::ranges::lower_bound(dialogue_messages_, id.value, {}, [](const DialogueMessage& entry) {
        return entry.id.value;
    });
    if (message == dialogue_messages_.end() || message->id != id) {
        throw std::out_of_range("unknown dialogue message id: " + std::to_string(id.value));
    }
    return *message;
}

std::span<const TileSheet> AssetCatalog::tile_sheets() const noexcept
{
    return tile_sheets_;
}

std::span<const FontSheet> AssetCatalog::font_sheets() const noexcept
{
    return font_sheets_;
}

} // namespace dw4::content