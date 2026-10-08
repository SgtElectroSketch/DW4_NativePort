#include "AdventureView.hpp"

#include <dw4/game/Render.hpp>

#include <stdexcept>
#include <string>
#include <algorithm>

namespace dw4::desktop {
namespace {

std::string operation_name(const game::LogOperation operation)
{
    switch (operation) {
    case game::LogOperation::load: return "CONTINUE A QUEST";
    case game::LogOperation::create: return "BEGIN A NEW QUEST";
    case game::LogOperation::copy: return "COPY A QUEST";
    case game::LogOperation::erase: return "ERASE A QUEST";
    case game::LogOperation::speed: return "CHANGE MESSAGE SPEED";
    }
    throw std::invalid_argument("unknown adventure-log operation");
}

} // namespace

AdventureView::AdventureView(const content::AssetCatalog& catalog)
    : palette_{{{0, 0, 0, 255}, {252, 252, 252, 255}, {252, 252, 252, 255}, {252, 252, 252, 255}}}
{
    if (catalog.font_sheets().empty()) throw std::runtime_error("adventure menus require the main font");
    glyphs_[' '] = 0;
    for (const auto& glyph : catalog.font_sheets()[0].glyphs) {
        if (glyph.meaning.size() == 1) glyphs_.try_emplace(glyph.meaning[0], glyph.tile);
    }
}

std::vector<std::uint16_t> AdventureView::name_characters() const
{
    std::vector<std::uint16_t> result;
    for (std::uint16_t glyph = 37; glyph <= 62; ++glyph) result.push_back(glyph);
    for (const auto glyph : {101, 106, 110, 109, 117, 118, 0}) result.push_back(static_cast<std::uint16_t>(glyph));
    for (std::uint16_t glyph = 11; glyph <= 36; ++glyph) result.push_back(glyph);
    for (const auto glyph : {119, 120, 0}) result.push_back(static_cast<std::uint16_t>(glyph));
    return result;
}

void AdventureView::glyphs(std::vector<game::TileDraw>& draws, const std::vector<std::uint16_t>& value, const int x, const int y) const
{
    game::TileDraw origin;
    origin.kind = game::GraphicKind::font;
    origin.x = x;
    origin.y = y;
    origin.palette = palette_;
    game::append_glyphs(draws, origin, value, static_cast<std::uint32_t>((256 - x) / 8), 1);
}

void AdventureView::text(std::vector<game::TileDraw>& draws, const std::string_view value, const int x, const int y,
                         const std::uint32_t columns) const
{
    std::vector<std::uint16_t> text;
    for (const auto character : value) {
        const auto glyph = glyphs_.find(character);
        text.push_back(glyph == glyphs_.end() ? glyphs_.at('?') : glyph->second);
    }
    game::TileDraw origin;
    origin.kind = game::GraphicKind::font;
    origin.x = x;
    origin.y = y;
    origin.palette = palette_;
    game::append_glyphs(draws, origin, text, std::min(columns, static_cast<std::uint32_t>((256 - x) / 8)),
                        static_cast<std::uint32_t>((240 - y) / 8));
}

std::vector<game::TileDraw> AdventureView::commands(const game::AdventureLog& menu) const
{
    std::vector<game::TileDraw> draws;
    const auto& state = menu.state();
    const auto window = [&](const int x, const int y, const std::uint32_t columns, const std::uint32_t rows,
                            const game::WindowTiles tiles = {134, 133, 137, 132, 0, 136, 135, 138, 139}) {
        game::TileDraw origin;
        origin.kind = game::GraphicKind::font;
        origin.x = x;
        origin.y = y;
        origin.palette = palette_;
        game::append_window(draws, origin, columns, rows, tiles);
    };
    const auto cursor = [&](const int x, const int y) {
        draws.push_back({game::GraphicKind::font, 0, 129, x, y, palette_});
    };
    const auto available_commands = menu.commands();
    window(16, 48, 24, static_cast<std::uint32_t>(available_commands.size() * 2U + 2U));
    std::size_t command_cursor = state.cursor;
    if (state.page != game::LogPage::commands) {
        const auto selected = std::ranges::find(available_commands, state.operation);
        command_cursor = selected == available_commands.end() ? 0 : static_cast<std::size_t>(selected - available_commands.begin());
    }
    for (std::size_t index = 0; index < available_commands.size(); ++index) {
        text(draws, operation_name(available_commands[index]), 32, 64 + static_cast<int>(index) * 16, 21);
    }
    if (!available_commands.empty()) cursor(24, 64 + static_cast<int>(command_cursor) * 16);
    if (state.page == game::LogPage::commands || state.page == game::LogPage::return_to_title) return draws;

    const auto slots = [&](const bool empty_selector, const bool active) {
        const int x = empty_selector ? 80 : 32;
        const int y = empty_selector ? 112 : 80;
        window(x, y, empty_selector ? 18 : 24, empty_selector ? 8 : 14);
        std::uint16_t selectable = 0;
        for (std::size_t slot = 0; slot < menu.slots().size(); ++slot) {
            const auto& record = menu.slots()[slot];
            const int row = y + 16 + static_cast<int>(slot) * (empty_selector ? 16 : 32);
            text(draws, "ADVENTURE LOG " + std::to_string(slot + 1), x + 16, row, 15);
            if (!empty_selector) {
                if (record.corrupt) text(draws, "Unreadable", x + 24, row + 8, 20);
                else if (record.occupied) {
                    glyphs(draws, record.name, x + 24, row + 8);
                    text(draws, "Chapter " + std::to_string(record.chapter), x + 104, row + 8, 12);
                }
            }
            const bool empty = !record.occupied && !record.corrupt;
            const bool available = (state.page == game::LogPage::select_destination || state.operation == game::LogOperation::create) ? empty :
                (state.operation == game::LogOperation::erase ? record.occupied || record.corrupt : record.occupied && !record.corrupt);
            if (active && available) { if (selectable == state.cursor) cursor(x + 8, row); ++selectable; }
            else if (!active && slot == state.source) cursor(x + 8, row);
        }
    };
    const bool destination = state.page == game::LogPage::select_destination ||
                             (state.operation == game::LogOperation::copy && state.page == game::LogPage::confirmation);
    slots(state.operation == game::LogOperation::create, state.page == game::LogPage::select_slot);
    if (destination) slots(true, state.page == game::LogPage::select_destination);
    if (state.page == game::LogPage::select_slot || state.page == game::LogPage::select_destination) return draws;

    const bool creating = state.operation == game::LogOperation::create;
    if (creating) {
        window(80, 32, 12, 4, {134, 133, 137, 132, 0, 136, 132, 0, 136});
        draws.push_back({game::GraphicKind::font, 0, 0, 104, 32, palette_});
        draws.push_back({game::GraphicKind::font, 0, 133, 104, 32, palette_, false, false, {104, 32, 7, 8}});
        text(draws, "NAME", 112, 32, 4);
        glyphs(draws, state.name, 96, 48);
        for (std::size_t index = 0; index < 8; ++index) {
            draws.push_back({game::GraphicKind::font, 0, index == state.name.size() ? 133U : 101U,
                             96 + static_cast<int>(index) * 8, 56, palette_});
        }
    }
    if (state.page == game::LogPage::name || (creating && state.page != game::LogPage::message)) {
        window(32, 64, 24, 14);
        const auto& characters = menu.characters();
        for (std::size_t index = 0; index < characters.size(); ++index) {
            draws.push_back({game::GraphicKind::font, 0, characters[index], 48 + static_cast<int>(index % 11) * 16,
                             80 + static_cast<int>(index / 11) * 16, palette_});
        }
        text(draws, "DEL", 160, 160);
        text(draws, "END", 192, 160);
        if (state.page == game::LogPage::name) {
            if (state.cursor < characters.size()) cursor(40 + (state.cursor % 11) * 16, 80 + (state.cursor / 11) * 16);
            else cursor(state.cursor == characters.size() ? 152 : 184, 160);
        }
    }
    if (state.page == game::LogPage::gender || (creating &&
        (state.page == game::LogPage::speed || state.page == game::LogPage::confirmation))) {
        window(144, 96, 10, 6);
        text(draws, "Male", 160, 112, 7); text(draws, "Female", 160, 128, 7);
        cursor(152, 112 + (state.page == game::LogPage::gender ? state.cursor : state.gender) * 16);
    }
    if (state.page == game::LogPage::speed || (state.page == game::LogPage::confirmation &&
               (creating || state.operation == game::LogOperation::speed))) {
        window(64, 112, 20, 14);
        text(draws, "Select the message", 72, 128, 18);
        text(draws, "speed you like.", 72, 144, 18);
        for (int speed = 0; speed < 8; ++speed) text(draws, std::to_string(speed + 1), 80 + speed * 16, 176, 1);
        text(draws, "Fast        Slow", 72, 192, 16);
        text(draws, "8: Manual", 72, 208, 16);
        const auto selected_speed = state.page == game::LogPage::speed ? state.cursor : state.speed;
        cursor(72 + selected_speed * 16, 176);
    }
    if (state.page == game::LogPage::confirmation) {
        if (state.operation == game::LogOperation::erase) {
            window(48, 96, 22, 10);
            text(draws, "ADVENTURE LOG " + std::to_string(state.source + 1), 64, 112, 18);
            text(draws, "Do you want to", 64, 128, 18);
            text(draws, "erase this", 64, 144, 18);
            text(draws, "quest?", 64, 160, 18);
        }
        window(192, 48, 6, 6);
        text(draws, "Yes", 208, 64, 3); text(draws, "No", 208, 80, 3);
        cursor(200, 64 + state.cursor * 16);
    } else if (state.page == game::LogPage::message || state.page == game::LogPage::ready) {
        window(16, 176, 28, 8);
        text(draws, state.message.substr(0, 78), 32, 192, 26);
    }
    return draws;
}

void AdventureView::draw(GraphicsRenderer& renderer, const game::AdventureLog& menu) const
{
    renderer.begin({0, 0, 0, 255});
    const auto draws = commands(menu);
    for (const auto& draw : draws) renderer.draw(draw);
}

} // namespace dw4::desktop