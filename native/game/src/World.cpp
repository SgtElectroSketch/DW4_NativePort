#include <dw4/game/World.hpp>

#include <array>
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace dw4::game {

MapPosition chapter_start_position(const std::uint8_t chapter)
{
    constexpr std::array<MapPosition, 5> starts{{{2, 1, 16, 21}, {1, 1, 9, 8}, {22, 1, 27, 5},
                                               {21, 3, 12, 15}, {20, 0, 5, 17}}};
    if (chapter == 0 || chapter > starts.size()) throw std::out_of_range("chapter must be 1 through 5");
    return starts[chapter - 1U];
}

AdventureState new_adventure(std::vector<std::uint16_t> hero_name, const std::uint8_t gender, const std::uint8_t speed)
{
    if (hero_name.empty() || hero_name.size() > 8 || gender > 1 || speed > 7 ||
        std::ranges::any_of(hero_name, [](const auto glyph) { return glyph > 140; }) ||
        std::ranges::none_of(hero_name, [](const auto glyph) { return glyph != 0; })) {
        throw std::invalid_argument("new adventure requires a valid hero name, gender and message speed");
    }
    AdventureState state;
    state.hero_name = std::move(hero_name);
    state.hero_gender = gender;
    state.message_speed = speed;
    state.position = chapter_start_position(1);
    state.story_flags = {81, 86};
    for (std::uint8_t id = 0; id < state.characters.size(); ++id) state.characters[id].id = id;
    auto& ragnar = state.characters[6];
    ragnar.hp = ragnar.maximum_hp = 27;
    ragnar.strength = 7;
    ragnar.agility = 4;
    ragnar.vitality = 13;
    ragnar.intelligence = 1;
    ragnar.luck = 2;
    ragnar.inventory = {{2, EquipmentSlot::weapon}, {38, EquipmentSlot::body}};
    return state;
}

} // namespace dw4::game