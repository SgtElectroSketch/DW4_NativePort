#pragma once

#include <dw4/game/Random.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace dw4::game {

struct EncounterRules final {
    struct Group final {
        std::uint8_t rate{}, control{};
        std::array<std::uint8_t, 14> entries{}, weights{};
        std::array<std::uint8_t, 4> extra{};
    };
    std::array<std::uint8_t, 256> grid{};
    std::array<Group, 64> groups;
    std::array<std::array<std::uint8_t, 6>, 256> mixed;
    std::array<std::uint8_t, 8> terrain_rates{}, mixed_span{}, mixed_base{}, single_span{}, single_base{};
    std::array<std::uint8_t, 3> first_steps{};
};

struct EncounterGroup final { std::uint16_t monster{}; std::uint8_t count{}; };
struct EncounterFormation final { std::vector<EncounterGroup> groups; std::uint8_t control{}; };

[[nodiscard]] std::optional<EncounterFormation> roll_world_encounter(const EncounterRules& rules,
    std::uint8_t x, std::uint8_t y, std::uint8_t terrain, std::uint8_t steps_since_battle, bool night, Random& random);

} // namespace dw4::game