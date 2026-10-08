#include <dw4/game/Game.hpp>
#include <dw4/game/Render.hpp>

#include <limits>
#include <stdexcept>

namespace dw4::game {
namespace {

void validate_grid(const TileDraw& origin, const std::uint32_t columns, const std::uint32_t rows)
{
    if (columns == 0 || rows == 0 || columns > logical_width / 8U || rows > logical_height / 8U || origin.x < 0 || origin.y < 0 ||
        origin.x > static_cast<std::int32_t>(logical_width - columns * 8U) ||
        origin.y > static_cast<std::int32_t>(logical_height - rows * 8U)) {
        throw std::invalid_argument("render layout grid must fit the logical framebuffer");
    }
}

} // namespace

void append_metatile(std::vector<TileDraw>& output, const TileDraw& origin, const std::array<std::uint16_t, 4>& quadrants)
{
    if (origin.x > std::numeric_limits<std::int32_t>::max() - 8 || origin.y > std::numeric_limits<std::int32_t>::max() - 8) {
        throw std::invalid_argument("metatile coordinates would overflow");
    }
    for (std::uint32_t index = 0; index < 4; ++index) {
        auto command = origin;
        const auto column = index % 2U;
        const auto row = index / 2U;
        const auto source_column = origin.flip_horizontal ? 1U - column : column;
        const auto source_row = origin.flip_vertical ? 1U - row : row;
        command.tile = quadrants[source_row * 2U + source_column];
        command.x += static_cast<std::int32_t>(column * 8U);
        command.y += static_cast<std::int32_t>(row * 8U);
        output.push_back(command);
    }
}

void append_window(std::vector<TileDraw>& output, const TileDraw& origin, const std::uint32_t columns, const std::uint32_t rows,
                   const WindowTiles& tiles)
{
    validate_grid(origin, columns, rows);
    if (columns < 3 || rows < 3) {
        throw std::invalid_argument("render window requires at least three columns and rows");
    }
    for (std::uint32_t row = 0; row < rows; ++row) {
        for (std::uint32_t column = 0; column < columns; ++column) {
            auto command = origin;
            command.x += static_cast<std::int32_t>(column * 8U);
            command.y += static_cast<std::int32_t>(row * 8U);
            if (row == 0) command.tile = column == 0 ? tiles.top_left : (column + 1U == columns ? tiles.top_right : tiles.top);
            else if (row + 1U == rows) command.tile = column == 0 ? tiles.bottom_left : (column + 1U == columns ? tiles.bottom_right : tiles.bottom);
            else command.tile = column == 0 ? tiles.left : (column + 1U == columns ? tiles.right : tiles.fill);
            output.push_back(command);
        }
    }
}

void append_glyphs(std::vector<TileDraw>& output, const TileDraw& origin, const std::span<const std::uint16_t> glyphs,
                   const std::uint32_t columns, const std::uint32_t rows)
{
    validate_grid(origin, columns, rows);
    if (glyphs.size() > columns * rows) {
        throw std::invalid_argument("glyph run exceeds its explicit layout grid");
    }
    for (std::size_t index = 0; index < glyphs.size(); ++index) {
        auto command = origin;
        command.tile = glyphs[index];
        command.x += static_cast<std::int32_t>((index % columns) * 8U);
        command.y += static_cast<std::int32_t>((index / columns) * 8U);
        output.push_back(command);
    }
}

} // namespace dw4::game