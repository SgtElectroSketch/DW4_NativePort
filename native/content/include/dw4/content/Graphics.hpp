#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace dw4::content {

enum class TilePixelFormat : std::uint8_t {
    monochrome,
    indexed_four_color,
};

struct TileSheet final {
    std::filesystem::path image;
    TilePixelFormat format{};
    std::uint32_t tile_count{};
};

struct FontGlyph final {
    std::uint16_t tile{};
    std::string meaning;
};

struct FontSheet final {
    std::filesystem::path image;
    std::vector<FontGlyph> glyphs;
};

} // namespace dw4::content