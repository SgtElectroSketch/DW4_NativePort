#include <dw4/game/Battle.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace dw4::game {

std::uint16_t physical_damage(const std::uint16_t attack, const std::uint16_t defense, Random& random)
{
    const auto reduced = static_cast<std::int32_t>(attack) - defense / 2;
    const auto base = reduced > 0 ? static_cast<std::uint32_t>(reduced / 2) : 0U;
    if (base < 8) {
        const auto rolled = static_cast<std::int32_t>(base) + static_cast<std::int32_t>(random.next_below(3)) - 1;
        return static_cast<std::uint16_t>(std::max(0, rolled));
    }
    return static_cast<std::uint16_t>(base * 7U / 8U + random.next_byte() * (base / 4U + 1U) / 256U);
}

Battle::Battle(std::vector<MonsterRule> enemies, AdventureState& adventure, const LevelCurve& curve, std::vector<ItemRule> items)
    : adventure_(adventure), curve_(curve), items_(std::move(items))
{
    if (enemies.empty() || enemies.size() > 8 || adventure.party[0] >= adventure.characters.size() || adventure.characters[adventure.party[0]].hp == 0) {
        throw std::invalid_argument("battle requires living party and bounded opponents");
    }
    for (auto& enemy : enemies) {
        if (enemy.hp == 0 || enemy.hp > 1023 || enemy.attack > 1023 || enemy.defense > 1023 || enemy.drop_denominator == 0) throw std::invalid_argument("invalid battle monster rule");
        monsters_.push_back({std::move(enemy), 0});
        monsters_.back().hp = monsters_.back().rule.hp;
    }
}

std::vector<BattleEvent> Battle::turn(const BattleCommand command, const std::size_t target, Random& random)
{
    if (outcome_ != BattleOutcome::ongoing) return {};
    if (command == BattleCommand::attack && (target >= monsters_.size() || monsters_[target].hp == 0)) throw std::invalid_argument("battle target is not alive");
    auto& character = adventure_.characters[adventure_.party[0]];
    std::vector<BattleEvent> events;
    bool defended = command == BattleCommand::defend;
    if (command == BattleCommand::flee) {
        ++escapes_;
        if (escapes_ >= 4 || random.next_below(256) < 128U) { outcome_ = BattleOutcome::escaped; return {{BattleEventKind::escaped}}; }
        events.push_back({BattleEventKind::escape_failed});
    } else if (defended) events.push_back({BattleEventKind::defend, character.id});
    struct Action final { bool player; std::size_t index; std::uint32_t initiative; };
    std::vector<Action> actions;
    if (command == BattleCommand::attack) actions.push_back({true, target, character.agility / 4U + random.next_below(std::max<std::uint32_t>(1, character.agility - character.agility / 4U))});
    for (std::size_t index = 0; index < monsters_.size(); ++index) if (monsters_[index].hp != 0) {
        const auto agility = monsters_[index].rule.agility;
        actions.push_back({false, index, agility / 4U + random.next_below(std::max<std::uint32_t>(1, agility - agility / 4U))});
    }
    std::ranges::stable_sort(actions, [](const auto& first, const auto& second) { return first.initiative > second.initiative; });
    for (const auto& action : actions) {
        if (character.hp == 0) break;
        auto& enemy = monsters_[action.index];
        if (enemy.hp == 0) continue;
        if (action.player) {
            const auto damage = physical_damage(attack_power(character, items_), enemy.rule.defense, random);
            enemy.hp = static_cast<std::uint16_t>(damage >= enemy.hp ? 0 : enemy.hp - damage);
            events.push_back({damage == 0 ? BattleEventKind::miss : BattleEventKind::attack, enemy.rule.id, damage});
            if (enemy.hp == 0) events.push_back({BattleEventKind::defeated, enemy.rule.id});
        } else {
            auto damage = physical_damage(enemy.rule.attack, defense_power(character, items_), random);
            if (defended) damage = static_cast<std::uint16_t>(damage / 2U);
            character.hp = static_cast<std::uint16_t>(damage >= character.hp ? 0 : character.hp - damage);
            events.push_back({damage == 0 ? BattleEventKind::enemy_miss : BattleEventKind::enemy_attack, enemy.rule.id, damage});
        }
    }
    if (character.hp == 0) outcome_ = BattleOutcome::defeat;
    else if (std::ranges::none_of(monsters_, [](const auto& enemy) { return enemy.hp != 0; })) resolve_victory(random, events);
    return events;
}

void Battle::resolve_victory(Random& random, std::vector<BattleEvent>& events)
{
    std::uint32_t experience = 0, gold = 0;
    for (const auto& enemy : monsters_) { experience += enemy.rule.experience; gold += enemy.rule.gold; }
    gain_gold(adventure_, gold);
    auto& character = adventure_.characters[adventure_.party[0]];
    const auto levels = award_experience(character, curve_, experience, random);
    events.push_back({BattleEventKind::rewards, static_cast<std::uint16_t>(std::min<std::uint32_t>(65535, experience)), static_cast<std::uint16_t>(std::min<std::uint32_t>(65535, gold))});
    for (const auto& level : levels) events.push_back({BattleEventKind::level_up, character.id, level.level});
    for (const auto& enemy : monsters_) if (enemy.rule.drop && character.inventory.size() < 8 &&
        random.next_below(enemy.rule.drop_denominator) < enemy.rule.drop_numerator) {
        character.inventory.push_back({*enemy.rule.drop, EquipmentSlot::none});
        events.push_back({BattleEventKind::dropped_item, *enemy.rule.drop});
        break;
    }
    outcome_ = BattleOutcome::victory;
}

BattleOutcome Battle::outcome() const noexcept { return outcome_; }
const std::vector<BattleMonster>& Battle::monsters() const noexcept { return monsters_; }

} // namespace dw4::game