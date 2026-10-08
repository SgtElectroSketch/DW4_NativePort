#include <dw4/game/Progression.hpp>

#include <algorithm>
#include <stdexcept>

namespace dw4::game {
namespace {

void validate_curve(const CharacterState& character, const LevelCurve& curve)
{
    if (character.id != curve.character || character.level == 0 || character.level > 99 || curve.levels[0].experience != 0) {
        throw std::invalid_argument("character and progression curve are incompatible");
    }
    for (std::size_t index = 1; index < curve.levels.size(); ++index) {
        if (curve.levels[index].experience <= curve.levels[index - 1].experience || curve.levels[index].experience > 9999999) {
            throw std::invalid_argument("experience thresholds must increase within the native save limit");
        }
    }
}

} // namespace

std::uint8_t growth_random_factor(Random& random)
{
    for (std::uint32_t attempt = 0; attempt < 64; ++attempt) {
        std::uint32_t total = 16;
        for (std::uint32_t draw = 0; draw < 32; ++draw) total += random.next_below(16);
        if (total >= 128 && total < 384) return static_cast<std::uint8_t>(total - 128U);
    }
    throw std::runtime_error("growth sampling exceeded its bounded rejection limit");
}

std::vector<LevelUp> award_experience(CharacterState& character, const LevelCurve& curve,
                                    const std::uint32_t experience, Random& random)
{
    validate_curve(character, curve);
    character.experience = static_cast<std::uint32_t>(std::min<std::uint64_t>(9999999, static_cast<std::uint64_t>(character.experience) + experience));
    std::vector<LevelUp> result;
    while (character.level < 99 && character.experience >= curve.levels[character.level].experience) {
        const auto& target = curve.levels[character.level];
        ++character.level;
        LevelUp raised{character.level, {}};
        const std::array<std::uint16_t*, 7> values{&character.strength, &character.agility, &character.vitality,
            &character.intelligence, &character.luck, &character.maximum_hp, &character.maximum_mp};
        std::uint16_t vitality_gain = 0;
        for (std::size_t stat = 0; stat < values.size(); ++stat) {
            if (stat == 6 && character.id >= 5) continue;
            const auto current = *values[stat];
            const auto ceiling = static_cast<std::uint32_t>(target.targets[stat]) + character.level / 2U + 10U;
            std::uint16_t gain;
            if (current >= ceiling) gain = static_cast<std::uint16_t>(random.next_below(2));
            else {
                const auto nominal = stat == 5 ? vitality_gain : target.gains[stat];
                gain = static_cast<std::uint16_t>(nominal * static_cast<std::uint32_t>(growth_random_factor(random)) / 121U);
            }
            if (stat == 2) vitality_gain = static_cast<std::uint16_t>(gain * 2U);
            const std::uint16_t cap = stat >= 5 ? 999 : 255;
            *values[stat] = static_cast<std::uint16_t>(std::min<std::uint32_t>(cap, current + gain));
            raised.gains[stat] = static_cast<std::uint16_t>(*values[stat] - current);
        }
        result.push_back(raised);
    }
    return result;
}

std::uint32_t experience_to_next_level(const CharacterState& character, const LevelCurve& curve)
{
    validate_curve(character, curve);
    if (character.level == 99) return 0;
    const auto threshold = curve.levels[character.level].experience;
    return character.experience >= threshold ? 0 : threshold - character.experience;
}

void gain_gold(AdventureState& state, const std::uint32_t amount) noexcept
{
    state.gold = static_cast<std::uint32_t>(std::min<std::uint64_t>(9999999, static_cast<std::uint64_t>(state.gold) + amount));
}

bool spend_gold(AdventureState& state, const std::uint32_t amount) noexcept
{
    if (state.gold < amount) return false;
    state.gold -= amount;
    return true;
}

} // namespace dw4::game