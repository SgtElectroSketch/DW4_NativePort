#pragma once

#include "GraphicsRenderer.hpp"

#include <dw4/game/AdventureLog.hpp>

#include <map>
#include <string_view>

namespace dw4::desktop {

class AdventureView final {
public:
    explicit AdventureView(const content::AssetCatalog& catalog);
    void draw(GraphicsRenderer& renderer, const game::AdventureLog& menu) const;
    [[nodiscard]] std::vector<game::TileDraw> commands(const game::AdventureLog& menu) const;
    [[nodiscard]] std::vector<std::uint16_t> name_characters() const;

private:
    void text(std::vector<game::TileDraw>& draws, std::string_view value, int x, int y, std::uint32_t columns = 26) const;
    void glyphs(std::vector<game::TileDraw>& draws, const std::vector<std::uint16_t>& value, int x, int y) const;
    std::map<char, std::uint16_t> glyphs_;
    game::Palette palette_;
};

} // namespace dw4::desktop