#pragma once

#include "GraphicsRenderer.hpp"

#include <dw4/content/FieldData.hpp>
#include <dw4/game/Field.hpp>
#include <dw4/game/Dialogue.hpp>

#include <map>
#include <string_view>

namespace dw4::desktop {

class FieldView final {
public:
    FieldView(const content::AssetCatalog& catalog, const content::FieldData& data, std::uint32_t atlas);
    void draw(GraphicsRenderer& renderer, const game::FieldState& state, const game::Dialogue* dialogue = nullptr) const;
    void text(GraphicsRenderer& renderer, std::string_view value, int x, int y, std::uint32_t columns = 26) const;
    void window(GraphicsRenderer& renderer, int x, int y, std::uint32_t columns, std::uint32_t rows) const;
    [[nodiscard]] std::vector<game::TerrainMap> rules() const;
    void monster(GraphicsRenderer& renderer, std::uint16_t id, int x, int y) const;
    void cursor(GraphicsRenderer& renderer, int x, int y) const;

private:
    void draw_dialogue(GraphicsRenderer& renderer, const game::Dialogue& dialogue, int x, int y,
                       std::uint32_t columns, std::uint32_t rows) const;
    void sprite(GraphicsRenderer& renderer, std::uint16_t id, game::Direction facing, int frame, int x, int y) const;
    const content::FieldData& data_;
    std::uint32_t atlas_{};
    std::map<char, std::uint16_t> glyphs_;
    game::Palette font_palette_;
};

} // namespace dw4::desktop