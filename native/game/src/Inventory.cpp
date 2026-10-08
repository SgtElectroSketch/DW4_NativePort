#include <dw4/game/Inventory.hpp>
#include <dw4/game/Progression.hpp>

#include <algorithm>
#include <stdexcept>

namespace dw4::game {
namespace {

const ItemRule& rule(const std::uint16_t id, const std::span<const ItemRule> items)
{
    const auto found = std::ranges::find_if(items, [&](const auto& item) { return item.id == id; });
    if (found == items.end()) throw std::invalid_argument("item rule is unavailable");
    return *found;
}

} // namespace

InventoryResult buy_item(AdventureState& adventure, const std::uint8_t character, const ItemRule& item)
{
    if (character >= adventure.characters.size()) return InventoryResult::invalid_character;
    if (item.id > 126 || item.price == 0) return InventoryResult::invalid_item;
    auto& inventory = adventure.characters[character].inventory;
    if (inventory.size() >= 8) return InventoryResult::full;
    if (adventure.gold < item.price) return InventoryResult::insufficient_gold;
    inventory.push_back({item.id, EquipmentSlot::none});
    static_cast<void>(spend_gold(adventure, item.price));
    return InventoryResult::success;
}

std::uint32_t sell_price(const ItemRule& item) noexcept { return item.price * 3U / 4U; }

InventoryResult collect_treasure(AdventureState& adventure, const std::uint16_t flag,
                                 const std::optional<std::uint16_t> item, const std::uint32_t gold)
{
    if (std::ranges::find(adventure.story_flags, flag) != adventure.story_flags.end()) return InventoryResult::invalid_item;
    const auto character = adventure.party[0];
    if (character >= adventure.characters.size()) return InventoryResult::invalid_character;
    auto& inventory = adventure.characters[character].inventory;
    if (item && inventory.size() >= 8) return InventoryResult::full;
    if (item && *item > 126) return InventoryResult::invalid_item;
    if (item) inventory.push_back({*item, EquipmentSlot::none});
    gain_gold(adventure, gold);
    adventure.story_flags.push_back(flag);
    return InventoryResult::success;
}

InventoryResult sell_item(AdventureState& adventure, const std::uint8_t character, const std::size_t slot, const ItemRule& item)
{
    if (character >= adventure.characters.size()) return InventoryResult::invalid_character;
    auto& inventory = adventure.characters[character].inventory;
    if (slot >= inventory.size() || inventory[slot].id != item.id || item.price == 0) return InventoryResult::invalid_item;
    if (item.cursed && inventory[slot].equipped != EquipmentSlot::none) return InventoryResult::cursed;
    inventory.erase(inventory.begin() + static_cast<std::ptrdiff_t>(slot));
    gain_gold(adventure, sell_price(item));
    return InventoryResult::success;
}

InventoryResult equip_item(AdventureState& adventure, const std::uint8_t character, const std::size_t slot,
                           const std::span<const ItemRule> items)
{
    if (character >= adventure.characters.size()) return InventoryResult::invalid_character;
    auto& inventory = adventure.characters[character].inventory;
    if (slot >= inventory.size()) return InventoryResult::invalid_item;
    const auto& selected = rule(inventory[slot].id, items);
    if (selected.slot == EquipmentSlot::none || (selected.equip_mask & (1U << (character == 8 ? 0 : character))) == 0) return InventoryResult::cannot_equip;
    if ((selected.id == 0x30 || selected.id == 0x31 || selected.id == 0x3B || selected.id == 0x4D) &&
        character != 2 && character != 3 && character != 7 && !(character == 0 && adventure.hero_gender == 1) && character != 8) return InventoryResult::cannot_equip;
    for (const auto& item : inventory) {
        if (item.equipped == selected.slot && rule(item.id, items).cursed) return InventoryResult::cursed;
    }
    for (auto& item : inventory) if (item.equipped == selected.slot) item.equipped = EquipmentSlot::none;
    inventory[slot].equipped = selected.slot;
    return InventoryResult::success;
}

InventoryResult transfer_item(AdventureState& adventure, const std::uint8_t from, const std::uint8_t to, const std::size_t slot)
{
    if (from >= adventure.characters.size() || to >= adventure.characters.size() || from == to) return InventoryResult::invalid_character;
    auto& source = adventure.characters[from].inventory;
    auto& target = adventure.characters[to].inventory;
    if (slot >= source.size()) return InventoryResult::invalid_item;
    if (target.size() >= 8) return InventoryResult::full;
    if (source[slot].equipped != EquipmentSlot::none) return InventoryResult::cannot_equip;
    target.push_back(source[slot]);
    source.erase(source.begin() + static_cast<std::ptrdiff_t>(slot));
    return InventoryResult::success;
}

std::uint16_t attack_power(const CharacterState& character, const std::span<const ItemRule> items)
{
    std::uint32_t value = character.strength;
    for (const auto& item : character.inventory) if (item.equipped == EquipmentSlot::weapon) value += rule(item.id, items).power;
    return static_cast<std::uint16_t>(std::min<std::uint32_t>(999, value));
}

InventoryResult discard_item(AdventureState& adventure, const std::uint8_t character, const std::size_t slot, const std::span<const ItemRule> items)
{
    if (character >= adventure.characters.size()) return InventoryResult::invalid_character;
    auto& inventory = adventure.characters[character].inventory;
    if (slot >= inventory.size()) return InventoryResult::invalid_item;
    const auto& selected = rule(inventory[slot].id, items);
    if (inventory[slot].equipped != EquipmentSlot::none && selected.cursed) return InventoryResult::cursed;
    inventory.erase(inventory.begin() + static_cast<std::ptrdiff_t>(slot));
    return InventoryResult::success;
}

std::uint16_t defense_power(const CharacterState& character, const std::span<const ItemRule> items)
{
    std::uint32_t value = character.agility / 2U;
    for (const auto& item : character.inventory) if (item.equipped != EquipmentSlot::none && item.equipped != EquipmentSlot::weapon) value += rule(item.id, items).power;
    return static_cast<std::uint16_t>(std::min<std::uint32_t>(999, value));
}

} // namespace dw4::game