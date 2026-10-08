#pragma once

#include <dw4/game/World.hpp>
#include <dw4/game/Random.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace dw4::game {

struct LevelTarget final {
    std::uint32_t experience{};
    std::array<std::uint16_t, 7> targets{};
    std::array<std::uint16_t, 7> gains{};
};

struct LevelCurve final {
    std::uint8_t character{};
    std::array<LevelTarget, 99> levels;
};

struct LevelUp final {
    std::uint8_t level{};
    std::array<std::uint16_t, 7> gains{};
};

[[nodiscard]] std::uint8_t growth_random_factor(Random& random);
[[nodiscard]] std::vector<LevelUp> award_experience(CharacterState& character, const LevelCurve& curve,
                                                  std::uint32_t experience, Random& random);
[[nodiscard]] std::uint32_t experience_to_next_level(const CharacterState& character, const LevelCurve& curve);
void gain_gold(AdventureState& state, std::uint32_t amount) noexcept;
[[nodiscard]] bool spend_gold(AdventureState& state, std::uint32_t amount) noexcept;

} // namespace dw4::game