#include <dw4/content/TitleData.hpp>

#include "JsonValidation.hpp"

#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace dw4::content {
namespace {

using detail::Json;
using detail::unsigned_field;
using detail::unsigned_value;

TitleColor color(const Json& record, const std::filesystem::path& path)
{
    if (!record.is_array() || record.size() != 4) {
        throw std::runtime_error("title color must contain four bytes: " + path.string());
    }
    TitleColor result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = static_cast<std::uint8_t>(unsigned_value(record[index], 255, "title.color", path));
    }
    return result;
}

TitleTile read_tile(const Json& draw, const TitleData& data, const std::filesystem::path& path)
{
    if (!draw.is_array() || draw.size() != 7 || !draw[1].is_number_integer() || !draw[2].is_number_integer()) {
        throw std::runtime_error("invalid title tile record: " + path.string());
    }
    const auto x = draw[1].get<std::int64_t>();
    const auto y = draw[2].get<std::int64_t>();
    if (x < -7 || x > 255 || y < -7 || y > 239) {
        throw std::runtime_error("title tile position is outside clipping bounds: " + path.string());
    }
    const auto tile = unsigned_value(draw[0], data.tiles - 1U, "title.tile", path);
    const auto palette = unsigned_value(draw[3], data.palettes.size() - 1U, "title.palette", path);
    const auto flips = unsigned_value(draw[4], 3, "title.flips", path);
    const auto top = unsigned_value(draw[5], 239, "title.clip_top", path);
    const auto height = unsigned_value(draw[6], 240, "title.clip_height", path);
    if (height == 0 || top + height > 240) {
        throw std::runtime_error("title clip rectangle is invalid: " + path.string());
    }
    return {static_cast<std::uint16_t>(tile), static_cast<std::int16_t>(x), static_cast<std::int16_t>(y),
            static_cast<std::uint16_t>(palette), static_cast<std::uint8_t>(flips),
            static_cast<std::uint16_t>(top), static_cast<std::uint16_t>(height)};
}

std::vector<TitleFrame> read_frames(const Json& records, const TitleData& data, const std::vector<TitleTile>& shared,
                                   const std::filesystem::path& path)
{
    if (!records.is_array() || records.size() > 3600) {
        throw std::runtime_error("title frame list is invalid or excessive: " + path.string());
    }
    std::vector<TitleFrame> result;
    result.reserve(records.size());
    for (const auto& record : records) {
        if (!record.is_object() || !record.contains("background") || !record.contains("draws")) {
            throw std::runtime_error("title frame fields are missing: " + path.string());
        }
        TitleFrame frame{color(record.at("background"), path), {}};
        const auto& draws = record.at("draws");
        if (!draws.is_array() || draws.size() > 4096) {
            throw std::runtime_error("title draws are invalid or excessive: " + path.string());
        }
        frame.draws.reserve(draws.size());
        for (const auto& draw : draws) {
            if (draw.is_number_integer() && !shared.empty()) {
                const auto index = unsigned_value(draw, shared.size() - 1U, "title.draw_reference", path);
                frame.draws.push_back(shared[static_cast<std::size_t>(index)]);
            } else {
                frame.draws.push_back(read_tile(draw, data, path));
            }
        }
        result.push_back(std::move(frame));
    }
    return result;
}

} // namespace

TitleData TitleData::load(const std::filesystem::path& directory)
{
    const auto path = directory / "index.json";
    std::error_code error;
    const auto bytes = std::filesystem::file_size(path, error);
    if (error || bytes > 64U * 1024U * 1024U) {
        throw std::runtime_error("title data is missing or exceeds 64 MiB; prepare native title assets: " + path.string());
    }
    std::ifstream stream(path);
    if (!stream) {
        throw std::runtime_error("cannot open title data: " + path.string());
    }
    Json document;
    try {
        stream >> document;
    } catch (const Json::parse_error&) {
        throw std::runtime_error("invalid title JSON: " + path.string());
    }
    stream >> std::ws;
    if (!stream.eof()) {
        throw std::runtime_error("trailing data after title JSON: " + path.string());
    }
    if (!document.is_object() || unsigned_field(document, "schema_version", 1, "title.schema_version", path) != 1 ||
        !document.contains("palettes") || !document.contains("frames") || !document.contains("exit_frames") ||
        !document.contains("image") || document.at("image") != "patterns.png") {
        throw std::runtime_error("unsupported or incomplete title data: " + path.string());
    }
    TitleData data;
    data.image = directory / "patterns.png";
    data.tiles = static_cast<std::uint32_t>(unsigned_field(document, "tiles", 8192, "title.tiles", path));
    data.artwork_frame = static_cast<std::uint32_t>(unsigned_field(document, "artwork_frame", 3600, "title.artwork_frame", path));
    data.fade_frames = static_cast<std::uint32_t>(unsigned_field(document, "fade_frames", 120, "title.fade_frames", path));
    data.loop_start = static_cast<std::uint32_t>(unsigned_field(document, "loop_start", 3600, "title.loop_start", path));
    data.loop_frames = static_cast<std::uint32_t>(unsigned_field(document, "loop_frames", 2048, "title.loop_frames", path));
    const auto& palettes = document.at("palettes");
    if (data.tiles == 0 || data.artwork_frame == 0 || data.fade_frames == 0 || data.loop_frames == 0 ||
        !palettes.is_array() || palettes.empty() || palettes.size() > 256) {
        throw std::runtime_error("invalid title timing, tile count or palettes: " + path.string());
    }
    for (const auto& palette : palettes) {
        if (!palette.is_array() || palette.size() != 4) {
            throw std::runtime_error("title palette must contain four colors: " + path.string());
        }
        data.palettes.push_back({color(palette[0], path), color(palette[1], path), color(palette[2], path), color(palette[3], path)});
    }
    std::vector<TitleTile> shared;
    if (const auto records = document.find("draw_records"); records != document.end()) {
        if (!records->is_array() || records->size() > 131072) {
            throw std::runtime_error("title shared draw records are invalid or excessive: " + path.string());
        }
        shared.reserve(records->size());
        for (const auto& record : *records) {
            shared.push_back(read_tile(record, data, path));
        }
    }
    data.frames = read_frames(document.at("frames"), data, shared, path);
    data.exit_frames = read_frames(document.at("exit_frames"), data, shared, path);
    if (const auto music = document.find("music"); music != document.end()) {
        if (!music->is_array() || music->size() > 16) {
            throw std::runtime_error("title music cues are invalid or excessive: " + path.string());
        }
        for (const auto& cue : *music) {
            if (!cue.is_object()) {
                throw std::runtime_error("title music cue must be an object: " + path.string());
            }
            const auto frame = unsigned_field(cue, "frame", 3600, "title.music.frame", path);
            const auto track = unsigned_field(cue, "track", 127, "title.music.track", path);
            if (track == 0 || (!data.music.empty() && frame <= data.music.back().frame)) {
                throw std::runtime_error("title music cues must be ordered and nonzero: " + path.string());
            }
            data.music.push_back({static_cast<std::uint32_t>(frame), static_cast<std::uint16_t>(track)});
        }
    }
    if (data.frames.empty() || data.artwork_frame > data.frames.size() || data.loop_start + data.loop_frames > data.frames.size() ||
        data.exit_frames.size() != data.fade_frames) {
        throw std::runtime_error("title animation range is inconsistent: " + path.string());
    }
    return data;
}

} // namespace dw4::content