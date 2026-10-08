#pragma once

#include <dw4/game/World.hpp>

#include <cstdint>
#include <optional>
#include <span>

namespace dw4::game {

struct ItemRule final {
    std::uint16_t id{}, power{};
    std::uint32_t price{};
    EquipmentSlot slot{};
    std::uint8_t equip_mask{};
    bool cursed{};
};

enum class InventoryResult : std::uint8_t { success, insufficient_gold, full, invalid_item, invalid_character, cannot_equip, cursed };

[[nodiscard]] InventoryResult buy_item(AdventureState& adventure, std::uint8_t character, const ItemRule& item);
[[nodiscard]] InventoryResult sell_item(AdventureState& adventure, std::uint8_t character, std::size_t slot, const ItemRule& item);
[[nodiscard]] InventoryResult equip_item(AdventureState& adventure, std::uint8_t character, std::size_t slot,
                                        std::span<const ItemRule> items);
[[nodiscard]] InventoryResult transfer_item(AdventureState& adventure, std::uint8_t from, std::uint8_t to, std::size_t slot);
[[nodiscard]] InventoryResult discard_item(AdventureState& adventure, std::uint8_t character, std::size_t slot, std::span<const ItemRule> items);
[[nodiscard]] std::uint16_t attack_power(const CharacterState& character, std::span<const ItemRule> items);
[[nodiscard]] std::uint16_t defense_power(const CharacterState& character, std::span<const ItemRule> items);
[[nodiscard]] std::uint32_t sell_price(const ItemRule& item) noexcept;
[[nodiscard]] InventoryResult collect_treasure(AdventureState& adventure, std::uint16_t flag,
                                             std::optional<std::uint16_t> item, std::uint32_t gold);

} // namespace dw4::game