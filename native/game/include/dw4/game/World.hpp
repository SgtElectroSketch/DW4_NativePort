#pragma once

#include <cstdint>
#include <array>
#include <vector>

namespace dw4::game {

struct MapPosition final {
    std::uint8_t map{};
    std::uint8_t submap{};
    std::int16_t x{};
    std::int16_t y{};
    bool operator==(const MapPosition&) const = default;
};

[[nodiscard]] MapPosition chapter_start_position(std::uint8_t chapter);

enum class Direction : std::uint8_t { up, right, down, left };
enum class EquipmentSlot : std::uint8_t { none, weapon, body, shield, head, accessory };

struct InventoryItem final {
    std::uint16_t id{};
    EquipmentSlot equipped{};
    bool operator==(const InventoryItem&) const = default;
};

struct CharacterState final {
    std::uint8_t id{};
    std::uint8_t level{1};
    std::uint16_t hp{};
    std::uint16_t maximum_hp{};
    std::uint16_t mp{};
    std::uint16_t maximum_mp{};
    std::uint16_t strength{};
    std::uint16_t agility{};
    std::uint16_t vitality{};
    std::uint16_t intelligence{};
    std::uint16_t luck{};
    std::uint32_t experience{};
    std::uint16_t conditions{};
    std::vector<InventoryItem> inventory;
    std::vector<std::uint16_t> spells;
    bool operator==(const CharacterState&) const = default;
};

struct AdventureState final {
    std::vector<std::uint16_t> hero_name;
    std::uint8_t hero_gender{};
    std::uint8_t message_speed{3};
    std::uint8_t chapter{1};
    MapPosition position;
    Direction facing{Direction::up};
    std::array<CharacterState, 9> characters;
    std::array<std::uint8_t, 4> party{6, 255, 255, 255};
    std::uint32_t gold{50};
    std::uint32_t bank_gold{};
    std::uint32_t casino_coins{};
    std::vector<InventoryItem> bag;
    std::vector<std::uint16_t> story_flags;
    bool operator==(const AdventureState&) const = default;
};

[[nodiscard]] AdventureState new_adventure(std::vector<std::uint16_t> hero_name, std::uint8_t gender, std::uint8_t speed);

} // namespace dw4::game