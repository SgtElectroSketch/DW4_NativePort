#include "FieldView.hpp"

#include <algorithm>
#include <stdexcept>

namespace dw4::desktop {
namespace {

game::Palette palette(const content::FieldPalette& source)
{
    game::Palette result;
    for (std::size_t index = 0; index < 4; ++index) result[index] = {source[index][0], source[index][1], source[index][2], source[index][3]};
    return result;
}

game::FieldEffect effect(const content::FieldActor::Effect& source)
{
    game::FieldEffect result{source.kind, source.argument, source.value, source.extra, source.invert};
    for (const auto& value : source.yes) result.yes.push_back(effect(value));
    for (const auto& value : source.no) result.no.push_back(effect(value));
    return result;
}

} // namespace

FieldView::FieldView(const content::AssetCatalog& catalog, const content::FieldData& data, const std::uint32_t atlas)
    : data_(data), atlas_(atlas), font_palette_{{{0, 0, 0, 255}, {252, 252, 252, 255}, {252, 252, 252, 255}, {252, 252, 252, 255}}}
{
    if (catalog.font_sheets().empty()) throw std::runtime_error("field view requires the main font");
    glyphs_[' '] = 0;
    for (const auto& glyph : catalog.font_sheets()[0].glyphs) if (glyph.meaning.size() == 1) glyphs_.try_emplace(glyph.meaning[0], glyph.tile);
}

std::vector<game::TerrainMap> FieldView::rules() const
{
    std::vector<game::TerrainMap> result;
    for (const auto& source : data_.maps()) {
        game::TerrainMap map;
        map.id = source.id; map.submap = source.submap; map.width = source.width; map.height = source.height; map.cells = source.cells;
        for (std::size_t slot = 0; slot < source.tiles.size(); ++slot) map.behaviors[slot] = source.tiles[slot].behavior;
        for (const auto& person : source.actors) {
            game::ActorDefinition actor{person.id, person.sprite, person.x, person.y, static_cast<game::Direction>(person.facing), person.motion};
            for (const auto& action : person.interaction) actor.interaction.push_back(effect(action));
            actor.patrol = person.patrol;
            actor.follow_player = person.follow_player;
            actor.remove_at_end = person.remove_at_end;
            map.actors.push_back(std::move(actor));
        }
        for (const auto& connection : source.connections) map.connections.push_back({connection.x, connection.y,
            {connection.map, connection.submap, connection.destination_x, connection.destination_y}});
        if (source.boundary_destination) {
            const auto& boundary = *source.boundary_destination;
            map.boundary_destination = game::MapPosition{boundary.map, boundary.submap, boundary.destination_x, boundary.destination_y};
        }
        result.push_back(std::move(map));
    }
    return result;
}

void FieldView::sprite(GraphicsRenderer& renderer, const std::uint16_t id, const game::Direction facing,
                       const int frame, const int x, const int y) const
{
    if (x <= -16 || y <= -16 || x >= 256 || y >= 240) return;
    const auto& source = data_.sprite(id).frames[static_cast<std::size_t>(facing) * 2U + static_cast<std::size_t>(frame & 1)];
    const auto colors = palette(data_.sprite_palettes()[source.palette]);
    for (std::size_t quadrant = 0; quadrant < 4; ++quadrant) {
        renderer.draw({game::GraphicKind::tile_sheet, atlas_, source.quadrants[quadrant],
                       x + static_cast<int>(quadrant % 2U) * 8, y + static_cast<int>(quadrant / 2U) * 8,
                       colors, source.flip_horizontal[quadrant]});
    }
}

void FieldView::monster(GraphicsRenderer& renderer, const std::uint16_t id, const int x, const int y) const
{
    const auto found = std::ranges::find_if(data_.monsters(), [&](const auto& value) { return value.id == id; });
    if (found == data_.monsters().end()) throw std::runtime_error("monster presentation is missing");
    for (const auto& draw : found->draws) renderer.draw({game::GraphicKind::tile_sheet, atlas_, draw.tile,
        x + draw.x, y + draw.y, palette(found->palettes[draw.palette]), (draw.flips & 1U) != 0, (draw.flips & 2U) != 0,
        {0, 40, 256, 104}});
}

void FieldView::cursor(GraphicsRenderer& renderer, const int x, const int y) const
{
    renderer.draw({game::GraphicKind::font, 0, 129, x, y, font_palette_});
}

void FieldView::window(GraphicsRenderer& renderer, const int x, const int y, const std::uint32_t columns, const std::uint32_t rows) const
{
    game::TileDraw origin{game::GraphicKind::font, 0, 0, x, y, font_palette_};
    std::vector<game::TileDraw> draws;
    game::append_window(draws, origin, columns, rows, {134, 133, 137, 132, 0, 136, 135, 138, 139});
    for (const auto& draw : draws) renderer.draw(draw);
}

void FieldView::text(GraphicsRenderer& renderer, const std::string_view value, const int x, const int y, const std::uint32_t columns) const
{
    std::vector<std::uint16_t> glyphs;
    for (const auto character : value) {
        const auto found = glyphs_.find(character);
        glyphs.push_back(found == glyphs_.end() ? glyphs_.at('?') : found->second);
    }
    game::TileDraw origin{game::GraphicKind::font, 0, 0, x, y, font_palette_};
    std::vector<game::TileDraw> draws;
    game::append_glyphs(draws, origin, glyphs, std::min(columns, static_cast<std::uint32_t>((256 - x) / 8)),
                       static_cast<std::uint32_t>((240 - y) / 8));
    for (const auto& draw : draws) renderer.draw(draw);
}

void FieldView::draw(GraphicsRenderer& renderer, const game::FieldState& state, const game::Dialogue* dialogue) const
{
    if (state.phase == 1 && state.event_step == 0) {
        renderer.begin({0, 0, 0, 255});
        if (dialogue != nullptr && !dialogue->finished()) draw_dialogue(renderer, *dialogue, 32, 144, 24, 10);
        return;
    }
    const auto& position = state.adventure.position;
    const auto& map = data_.map(position.map, position.submap);
    renderer.begin(palette(map.palettes[0])[0]);
    const int player_x = position.x * 16 + state.player_offset_x;
    const int player_y = position.y * 16 + state.player_offset_y;
    const int camera_x = player_x - 120;
    const int camera_y = player_y - 112;
    const auto roof = position.map == 255 ? 0 : map.cells[static_cast<std::size_t>(position.y) * map.width + position.x] >> 5;
    const int first_x = camera_x >= 0 ? camera_x / 16 : (camera_x - 15) / 16;
    const int first_y = camera_y >= 0 ? camera_y / 16 : (camera_y - 15) / 16;
    std::vector<game::TileDraw> draws;
    for (int y = first_y; y <= first_y + 15; ++y) for (int x = first_x; x <= first_x + 16; ++x) {
        std::uint8_t cell = map.border;
        if (x >= 0 && y >= 0 && x < map.width && y < map.height) cell = map.cells[static_cast<std::size_t>(y) * map.width + x];
        std::size_t slot = position.map == 255 ? cell : cell & 31U;
        if (position.map != 255 && (cell >> 5) != 0 && (cell >> 5) != roof) slot = 32;
        const auto& tile = map.tiles.at(slot);
        game::TileDraw origin{game::GraphicKind::tile_sheet, atlas_, 0, x * 16 - camera_x, y * 16 - camera_y, palette(map.palettes[tile.palette])};
        game::append_metatile(draws, origin, tile.quadrants);
    }
    for (const auto& draw : draws) renderer.draw(draw);
    for (const auto& actor : state.actors) if (actor.active) {
        sprite(renderer, actor.sprite, actor.facing, actor.step / 4,
               actor.x * 16 - camera_x - (actor.x - actor.target_x) * actor.step * 2,
               actor.y * 16 - camera_y - (actor.y - actor.target_y) * actor.step * 2);
    }
    sprite(renderer, state.adventure.party[0], state.adventure.facing, state.player_step / 4, 120, 112);
    if (dialogue != nullptr && !dialogue->finished()) draw_dialogue(renderer, *dialogue, 16, 144, 28, 12);
}

void FieldView::draw_dialogue(GraphicsRenderer& renderer, const game::Dialogue& dialogue, const int x, const int y,
                              const std::uint32_t columns, const std::uint32_t rows) const
{
        window(renderer, x, y, columns, rows);
        std::size_t visible = dialogue.visible_glyphs();
        for (std::size_t row = 0; row < dialogue.page().lines.size(); ++row) {
            const auto& line = dialogue.page().lines[row];
            const auto count = std::min(visible, line.size());
            for (std::size_t column = 0; column < count; ++column) renderer.draw({game::GraphicKind::font, 0, line[column],
                x + 8 + static_cast<int>(column) * 8, y + 8 + static_cast<int>(row) * 8, font_palette_});
            visible -= count;
        }
        if (dialogue.waiting()) renderer.draw({game::GraphicKind::font, 0, 128, x + static_cast<int>(columns / 2U) * 8,
                                               y + static_cast<int>(rows - 2U) * 8, font_palette_});
}

} // namespace dw4::desktop