#pragma once

#include <dw4/game/Progression.hpp>
#include <dw4/game/Inventory.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace dw4::game {

struct MonsterRule final {
    std::uint16_t id{};
    std::uint16_t hp{}, mp{}, attack{}, defense{}, agility{};
    std::uint32_t experience{}, gold{};
    std::optional<std::uint16_t> drop;
    std::uint32_t drop_numerator{}, drop_denominator{1};
};

struct BattleMonster final {
    MonsterRule rule;
    std::uint16_t hp{};
    bool operator==(const BattleMonster&) const = delete;
};

enum class BattleCommand : std::uint8_t { attack, defend, flee };
enum class BattleOutcome : std::uint8_t { ongoing, victory, defeat, escaped };
enum class BattleEventKind : std::uint8_t { attack, miss, defend, enemy_attack, enemy_miss, defeated, escaped, escape_failed, level_up, rewards, dropped_item };

struct BattleEvent final {
    BattleEventKind kind{};
    std::uint16_t actor{}, amount{};
};

[[nodiscard]] std::uint16_t physical_damage(std::uint16_t attack, std::uint16_t defense, Random& random);

class Battle final {
public:
    Battle(std::vector<MonsterRule> enemies, AdventureState& adventure, const LevelCurve& curve, std::vector<ItemRule> items);
    [[nodiscard]] std::vector<BattleEvent> turn(BattleCommand command, std::size_t target, Random& random);
    [[nodiscard]] BattleOutcome outcome() const noexcept;
    [[nodiscard]] const std::vector<BattleMonster>& monsters() const noexcept;

private:
    void resolve_victory(Random& random, std::vector<BattleEvent>& events);
    std::vector<BattleMonster> monsters_;
    AdventureState& adventure_;
    const LevelCurve& curve_;
    std::vector<ItemRule> items_;
    BattleOutcome outcome_{};
    std::uint8_t escapes_{};
};

} // namespace dw4::game