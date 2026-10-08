#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace dw4::game {

struct Color final {
    std::uint8_t red{};
    std::uint8_t green{};
    std::uint8_t blue{};
    std::uint8_t alpha{255};

    [[nodiscard]] constexpr bool operator==(const Color&) const noexcept = default;
};

using Palette = std::array<Color, 4>;

enum class GraphicKind : std::uint8_t {
    tile_sheet,
    font,
};

struct ClipRect final {
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t width{256};
    std::int32_t height{240};
};

struct TileDraw final {
    GraphicKind kind{};
    std::uint32_t sheet{};
    std::uint32_t tile{};
    std::int32_t x{};
    std::int32_t y{};
    Palette palette;
    bool flip_horizontal{};
    bool flip_vertical{};
    ClipRect clip;
};

struct WindowTiles final {
    std::uint16_t top_left{};
    std::uint16_t top{};
    std::uint16_t top_right{};
    std::uint16_t left{};
    std::uint16_t fill{};
    std::uint16_t right{};
    std::uint16_t bottom_left{};
    std::uint16_t bottom{};
    std::uint16_t bottom_right{};
};

void append_metatile(std::vector<TileDraw>& output, const TileDraw& origin, const std::array<std::uint16_t, 4>& quadrants);
void append_window(std::vector<TileDraw>& output, const TileDraw& origin, std::uint32_t columns, std::uint32_t rows,
                   const WindowTiles& tiles);
void append_glyphs(std::vector<TileDraw>& output, const TileDraw& origin, std::span<const std::uint16_t> glyphs,
                   std::uint32_t columns, std::uint32_t rows);

} // namespace dw4::game