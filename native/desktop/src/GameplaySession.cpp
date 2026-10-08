#include "GameplaySession.hpp"

#include <dw4/game/AdventureLog.hpp>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

namespace dw4::desktop {
namespace {

game::FieldState restore(const content::AdventureSave& save)
{
    game::FieldState state;
    state.adventure = game::new_adventure(save.name, save.gender, save.message_speed);
    if (!save.world) { state.phase = 1; return state; }
    const auto& source = *save.world;
    auto& adventure = state.adventure;
    adventure.chapter = save.chapter;
    adventure.position = {source.map, source.submap, source.x, source.y};
    adventure.facing = static_cast<game::Direction>(source.facing);
    adventure.party = source.party;
    adventure.gold = source.gold; adventure.bank_gold = source.bank_gold; adventure.casino_coins = source.casino_coins;
    adventure.story_flags = source.story_flags;
    adventure.bag.clear();
    for (const auto& item : source.bag) adventure.bag.push_back({item.id, static_cast<game::EquipmentSlot>(item.equipped)});
    for (std::size_t id = 0; id < source.characters.size(); ++id) {
        const auto& from = source.characters[id];
        auto& to = adventure.characters[id];
        to.id = from.id; to.level = from.level; to.hp = from.hp; to.maximum_hp = from.maximum_hp; to.mp = from.mp; to.maximum_mp = from.maximum_mp;
        to.strength = from.strength; to.agility = from.agility; to.vitality = from.vitality; to.intelligence = from.intelligence; to.luck = from.luck;
        to.experience = from.experience; to.conditions = from.conditions; to.spells = from.spells; to.inventory.clear();
        for (const auto& item : from.inventory) to.inventory.push_back({item.id, static_cast<game::EquipmentSlot>(item.equipped)});
    }
    for (const auto& actor : source.actors) state.actors.push_back({actor.id, actor.sprite, actor.x, actor.y, actor.target_x, actor.target_y,
        static_cast<game::Direction>(actor.facing), actor.step, actor.instruction, actor.delay, actor.active});
    state.phase = source.phase; state.event_step = source.event_step; state.event_delay = source.event_delay;
    return state;
}

content::AdventureSave snapshot(const game::FieldState& state, const game::FrameState& frame)
{
    content::AdventureSave save;
    const auto& adventure = state.adventure;
    save.name = adventure.hero_name; save.gender = adventure.hero_gender; save.message_speed = adventure.message_speed; save.chapter = adventure.chapter;
    save.frame = frame.number; save.random_engine = frame.random.engine; save.random_draws = frame.random.draw_counter;
    save.world.emplace();
    auto& target = *save.world;
    target.map = adventure.position.map; target.submap = adventure.position.submap; target.x = adventure.position.x; target.y = adventure.position.y;
    target.facing = static_cast<std::uint8_t>(adventure.facing); target.party = adventure.party; target.gold = adventure.gold;
    target.bank_gold = adventure.bank_gold; target.casino_coins = adventure.casino_coins; target.story_flags = adventure.story_flags;
    for (const auto& item : adventure.bag) target.bag.push_back({item.id, static_cast<std::uint8_t>(item.equipped)});
    for (std::size_t id = 0; id < adventure.characters.size(); ++id) {
        const auto& from = adventure.characters[id];
        auto& to = target.characters[id];
        to.id = from.id; to.level = from.level; to.hp = from.hp; to.maximum_hp = from.maximum_hp; to.mp = from.mp; to.maximum_mp = from.maximum_mp;
        to.strength = from.strength; to.agility = from.agility; to.vitality = from.vitality; to.intelligence = from.intelligence; to.luck = from.luck;
        to.experience = from.experience; to.conditions = from.conditions; to.spells = from.spells;
        for (const auto& item : from.inventory) to.inventory.push_back({item.id, static_cast<std::uint8_t>(item.equipped)});
    }
    for (const auto& actor : state.actors) target.actors.push_back({actor.id, actor.sprite, actor.x, actor.y, actor.target_x, actor.target_y,
        static_cast<std::uint8_t>(actor.facing), actor.step, actor.instruction, actor.delay, actor.active});
    target.phase = state.phase; target.event_step = state.event_step; target.event_delay = state.event_delay;
    return save;
}

} // namespace

GameplaySession::GameplaySession(const content::AssetCatalog& catalog, const content::FieldData& data, const std::uint32_t atlas,
                                 const content::AdventureSave& save, const std::uint8_t slot, AdventureStore& store)
    : catalog_(catalog), data_(data), store_(store), slot_(slot), view_(catalog, data, atlas), field_(view_.rules(), restore(save)), fresh_(!save.world || save.world->phase != 0)
{
    for (const auto& item : data.items()) items_.push_back({item.id, item.power, item.price, static_cast<game::EquipmentSlot>(item.slot), item.equip_mask, item.cursed});
    for (const auto& curve : data.progression()) {
        auto& target = curves_[curve.id];
        target.character = curve.id;
        for (std::size_t level = 0; level < 99; ++level) target.levels[level] = {curve.levels[level].experience, curve.levels[level].targets, curve.levels[level].gains};
    }
    const auto& encounters = data.encounters();
    encounters_.grid = encounters.grid; encounters_.mixed = encounters.mixed; encounters_.terrain_rates = encounters.terrain_rates;
    encounters_.first_steps = encounters.first_steps; encounters_.mixed_span = encounters.mixed_span; encounters_.mixed_base = encounters.mixed_base;
    encounters_.single_span = encounters.single_span; encounters_.single_base = encounters.single_base;
    for (std::size_t index = 0; index < 64; ++index) encounters_.groups[index] = {encounters.groups[index].rate, encounters.groups[index].control,
        encounters.groups[index].entries, encounters.groups[index].weights, encounters.groups[index].extra};
    if (items_.empty() || data.progression().size() != 9) throw std::runtime_error("gameplay requires complete item and progression data");
    if (fresh_) begin_opening();
}

content::AdventureSave GameplaySession::create_save(const game::LogRequest& request, const game::FrameState& frame)
{
    game::FieldState state;
    state.adventure = game::new_adventure(request.name, request.gender, request.speed);
    state.phase = 1;
    return snapshot(state, frame);
}

std::vector<std::uint16_t> GameplaySession::text_glyphs(const std::string_view text) const
{
    std::vector<std::uint16_t> result;
    for (const auto character : text) {
        if (character == ' ') result.push_back(0);
        else if (character >= '0' && character <= '9') result.push_back(static_cast<std::uint16_t>(character - '0' + 1));
        else if (character >= 'a' && character <= 'z') result.push_back(static_cast<std::uint16_t>(character - 'a' + 11));
        else if (character >= 'A' && character <= 'Z') result.push_back(static_cast<std::uint16_t>(character - 'A' + 37));
        else {
            const auto& glyphs = catalog_.font_sheets()[0].glyphs;
            const auto found = std::ranges::find_if(glyphs, [&](const auto& glyph) { return glyph.meaning.size() == 1 && glyph.meaning[0] == character; });
            if (found == glyphs.end()) throw std::invalid_argument("text character has no native glyph");
            result.push_back(found->tile);
        }
    }
    return result;
}

game::DialogueContext GameplaySession::dialogue_context() const
{
    game::DialogueContext context;
    context.party = field_.state().adventure.party;
    constexpr std::array<std::string_view, 9> names{"Hero", "Cristo", "Brey", "Mara", "Nara", "Taloon", "Ragnar", "Alena", "Hero"};
    for (std::size_t id = 0; id < names.size(); ++id) context.character_names[id] = text_glyphs(names[id]);
    context.character_names[0] = context.character_names[8] = field_.state().adventure.hero_name;
    for (const auto& item : data_.items()) if (!item.name.empty()) context.lookup_names[item.id] = text_glyphs(item.name);
    return context;
}

void GameplaySession::show_message(const std::uint16_t id)
{
    const auto& message = catalog_.dialogue_message({id});
    dialogue_ = std::make_unique<game::Dialogue>(game::layout_dialogue(message.symbols, dialogue_context(), id == 0x300 ? 22 : 26, 8), field_.state().adventure.message_speed);
}

void GameplaySession::show_text(const std::string_view text)
{
    const auto glyphs = text_glyphs(text);
    std::vector<std::uint8_t> symbols;
    for (const auto glyph : glyphs) symbols.push_back(static_cast<std::uint8_t>(glyph));
    dialogue_ = std::make_unique<game::Dialogue>(game::layout_dialogue(symbols, dialogue_context(), 26, 8), std::uint8_t{0});
}

void GameplaySession::begin_opening()
{
    field_.state_for_session().phase = 1;
    opening_step_ = field_.state().event_step;
    show_message(opening_step_ == 0 ? 0x300 : opening_step_ == 1 ? 0x301 : 0x302);
}

void GameplaySession::advance_opening()
{
    ++opening_step_;
    auto& state = field_.state_for_session();
    state.event_step = static_cast<std::uint16_t>(opening_step_);
    if (opening_step_ == 1) { show_message(0x301); return; }
    if (opening_step_ == 2) {
        const auto king = std::ranges::find_if(state.actors, [](const auto& actor) { return actor.id == 20; });
        if (king != state.actors.end()) { king->x = king->target_x = 16; king->y = king->target_y = 19; king->facing = game::Direction::down; }
        show_message(0x302); return;
    }
    field_.set_flag(28, true);
    state.phase = 0;
    fresh_ = false;
}

void GameplaySession::open_interaction(const std::uint16_t actor)
{
    const auto found = std::ranges::find_if(field_.map().actors, [&](const auto& definition) { return definition.id == actor; });
    if (found == field_.map().actors.end()) throw std::runtime_error("actor interaction program is unavailable");
    interaction_ = std::make_unique<game::Interaction>(found->interaction, actor, field_.state_for_session().adventure);
    advance_interaction();
}

void GameplaySession::advance_interaction(const std::optional<bool> response)
{
    const auto request = interaction_->next(response);
    if (!request) { if (interaction_->finished()) interaction_.reset(); page_ = Page::field; return; }
    switch (request->kind) {
    case game::InteractionRequestKind::message: show_message(request->value); break;
    case game::InteractionRequestKind::question: page_ = Page::question; cursor_ = 0; break;
    case game::InteractionRequestKind::shop: open_shop(request->value); break;
    case game::InteractionRequestKind::callback:
        if (request->value == 0) {
            if (field_.state().adventure.position.y == 22) show_message(1931);
            else open_shop(3);
        }
        else {
            if (request->value == 1) field_.restart_actor(8);
            else if (request->value == 2) field_.refresh_actors();
            else throw std::runtime_error("actor callback semantics are unavailable");
            advance_interaction(true);
        }
        break;
    }
}

const content::FieldItem& GameplaySession::item(const std::uint16_t id) const
{
    const auto found = std::ranges::find_if(data_.items(), [&](const auto& value) { return value.id == id; });
    if (found == data_.items().end()) throw std::runtime_error("item data is missing");
    return *found;
}

void GameplaySession::open_shop(const std::uint16_t mode)
{
    shop_mode_ = mode; cursor_ = 0;
    if (mode == 5) page_ = Page::church;
    else if (mode == 6) {
        const auto& position = field_.state().adventure.position;
        const auto inn = std::ranges::find_if(data_.inns(), [&](const auto& value) { return value.map == position.map && value.submap == position.submap; });
        if (inn == data_.inns().end()) throw std::runtime_error("inn price data is unavailable");
        inn_price_ = static_cast<std::uint32_t>(inn->price) * static_cast<std::uint32_t>(std::ranges::count_if(field_.state().adventure.party, [](const auto id) { return id != 255; }));
        page_ = Page::inn;
    }
    else {
        const auto& position = field_.state().adventure.position;
        const auto stock = std::ranges::find_if(data_.shops(), [&](const auto& shop) { return shop.map == position.map && shop.submap == position.submap && shop.mode == mode; });
        if (stock == data_.shops().end()) throw std::runtime_error("merchant stock is unavailable");
        stock_ = stock->items;
        page_ = Page::trade;
    }
}

void GameplaySession::try_encounter(game::Game& game)
{
    const auto& position = field_.state().adventure.position;
    if (position.map != 255) return;
    const auto& map = field_.map();
    const auto terrain = map.cells[static_cast<std::size_t>(position.y) * map.width + position.x];
    game::Random random(game.frame().random);
    const auto formation = game::roll_world_encounter(encounters_, static_cast<std::uint8_t>(position.x), static_cast<std::uint8_t>(position.y), terrain, encounter_steps_, false, random);
    if (encounter_steps_ < 255) ++encounter_steps_;
    if (formation) {
        std::vector<game::MonsterRule> enemies;
        for (const auto& group : formation->groups) {
            const auto monster = std::ranges::find_if(data_.monsters(), [&](const auto& value) { return value.id == group.monster; });
            if (monster == data_.monsters().end()) throw std::runtime_error("encounter monster rules are unavailable");
            for (std::uint8_t count = 0; count < group.count && enemies.size() < 8; ++count) enemies.push_back({monster->id, monster->hp, monster->mp,
                monster->attack, monster->defense, monster->agility, monster->experience, monster->gold, monster->drop,
                monster->drop_numerator, monster->drop_denominator});
        }
        auto& adventure = field_.state_for_session().adventure;
        battle_ = std::make_unique<game::Battle>(std::move(enemies), adventure, curves_[adventure.party[0]], items_);
        page_ = Page::battle; cursor_ = 0; encounter_steps_ = 0;
        show_text("Monsters appear!");
    }
    auto frame = game.frame(); frame.random = random.state(); game = game::Game(std::move(frame));
    if (formation) store_.write(slot_, snapshot(field_.state(), game.frame()));
}

void GameplaySession::battle_turn(const game::BattleCommand command, const std::size_t target, game::Game& game)
{
    game::Random random(game.frame().random);
    const auto events = battle_->turn(command, target, random);
    std::string report;
    const auto name = [&](const std::uint16_t id) {
        const auto monster = std::ranges::find_if(data_.monsters(), [&](const auto& value) { return value.id == id; });
        return monster == data_.monsters().end() ? std::string("Ragnar") : monster->name;
    };
    for (const auto& event : events) {
        if (!report.empty()) report += ' ';
        switch (event.kind) {
        case game::BattleEventKind::attack: report += "Ragnar attacks " + name(event.actor) + " for " + std::to_string(event.amount) + " damage."; break;
        case game::BattleEventKind::miss: report += "Ragnar's attack misses."; break;
        case game::BattleEventKind::defend: report += "Ragnar defends."; break;
        case game::BattleEventKind::enemy_attack: report += name(event.actor) + " attacks for " + std::to_string(event.amount) + " damage."; break;
        case game::BattleEventKind::enemy_miss: report += name(event.actor) + " misses."; break;
        case game::BattleEventKind::defeated: report += name(event.actor) + " is defeated."; break;
        case game::BattleEventKind::escaped: report += "Ragnar escapes."; break;
        case game::BattleEventKind::escape_failed: report += "The way is blocked."; break;
        case game::BattleEventKind::level_up: report += "Ragnar reaches level " + std::to_string(event.amount) + "."; break;
        case game::BattleEventKind::rewards: report += "Ragnar gains " + std::to_string(event.actor) + " EXP and " + std::to_string(event.amount) + " gold."; break;
        case game::BattleEventKind::dropped_item: report += "Found " + item(event.actor).name + "."; break;
        }
    }
    auto frame = game.frame(); frame.random = random.state(); game = game::Game(std::move(frame));
    page_ = Page::battle; cursor_ = 0;
    if (!report.empty()) show_text(report);
}

void GameplaySession::finish_battle(game::Game& game)
{
    if (battle_->outcome() == game::BattleOutcome::defeat) {
        auto& state = field_.state_for_session();
        auto& character = state.adventure.characters[state.adventure.party[0]];
        state.adventure.gold /= 2U; character.hp = character.maximum_hp; character.conditions = 0;
        state.adventure.position = game::chapter_start_position(1); state.actors.clear();
        field_ = game::Field(view_.rules(), state);
        show_text("Ragnar returns to Burland.");
    }
    battle_.reset(); page_ = Page::field;
    save(game.frame());
}

void GameplaySession::tick(game::Game& game)
{
    const auto actions = input_.tick(game.frame().input);
    if (dialogue_) {
        dialogue_->tick(game.frame().pressed);
        if (dialogue_->finished()) {
            dialogue_.reset();
            if (fresh_) advance_opening();
            else if (interaction_ && page_ == Page::field) advance_interaction(true);
        }
        return;
    }
    if (battle_ && battle_->outcome() != game::BattleOutcome::ongoing) { finish_battle(game); return; }
    auto& adventure = field_.state_for_session().adventure;
    const auto member = adventure.party[0];
    auto& character = adventure.characters[member];
    if (page_ == Page::field) {
        const auto previous_step = field_.state().player_step;
        field_.tick(game.frame().input);
        if (previous_step > 0 && field_.state().player_step == 0) try_encounter(game);
        if (battle_) return;
        if (actions.confirm && field_.state().player_step == 0) { page_ = Page::commands; cursor_ = 0; }
        if (game.frame().number > last_save_ + 300U && field_.state().player_step == 0) save(game.frame());
        return;
    }
    if (actions.cancel) {
        if (battle_) { page_ = Page::battle; cursor_ = 0; return; }
        if (interaction_) { interaction_.reset(); page_ = Page::field; }
        else page_ = page_ == Page::commands ? Page::field : Page::commands;
        cursor_ = 0; return;
    }
    std::size_t count = 0;
    if (page_ == Page::commands) count = 7;
    else if (page_ == Page::question) count = 2;
    else if (page_ == Page::inventory || page_ == Page::selling) count = character.inventory.size();
    else if (page_ == Page::item_action) count = 3;
    else if (page_ == Page::church) count = 3;
    else if (page_ == Page::inn) count = 2;
    else if (page_ == Page::merchant) count = stock_.size();
    else if (page_ == Page::trade) count = 3;
    else if (page_ == Page::battle) count = 3;
    else if (page_ == Page::target) count = battle_->monsters().size();
    if (count != 0) {
        if (actions.up || actions.left) cursor_ = static_cast<std::uint16_t>((cursor_ + count - 1U) % count);
        if (actions.down || actions.right) cursor_ = static_cast<std::uint16_t>((cursor_ + 1U) % count);
    }
    if (!actions.confirm) return;
    if (page_ == Page::battle) {
        if (cursor_ == 0) { page_ = Page::target; cursor_ = 0; while (cursor_ < battle_->monsters().size() && battle_->monsters()[cursor_].hp == 0) ++cursor_; }
        else battle_turn(cursor_ == 1 ? game::BattleCommand::defend : game::BattleCommand::flee, 0, game);
    } else if (page_ == Page::target) {
        if (battle_->monsters()[cursor_].hp != 0) battle_turn(game::BattleCommand::attack, cursor_, game);
    } else if (page_ == Page::trade) {
        if (cursor_ == 0) { page_ = Page::merchant; cursor_ = 0; }
        else if (cursor_ == 1) { page_ = Page::selling; cursor_ = 0; }
        else { interaction_.reset(); page_ = Page::field; cursor_ = 0; }
    } else if (page_ == Page::commands) {
        const auto command = cursor_; cursor_ = 0;
        if (command == 0) { page_ = Page::field; if (const auto actor = field_.actor_in_front()) open_interaction(*actor); else show_message(0x2F0); }
        else if (command == 1) page_ = Page::status;
        else if (command == 2) page_ = Page::inventory;
        else if (command == 3) { page_ = Page::field; show_text("Ragnar cannot use spells."); }
        else if (command == 4) {
            page_ = Page::field;
            const auto& position = adventure.position;
            const auto found = std::ranges::find_if(data_.treasures(), [&](const auto& treasure) {
                return treasure.map == position.map && treasure.submap == position.submap && treasure.x == position.x && treasure.y == position.y;
            });
            if (found == data_.treasures().end()) show_text("Nothing was found.");
            else {
                const auto result = game::collect_treasure(adventure, found->flag, found->item, found->gold);
                if (result == game::InventoryResult::full) show_text("Ragnar cannot carry any more.");
                else if (result != game::InventoryResult::success || (!found->item && found->gold == 0)) show_text("The chest is empty.");
                else if (found->item) show_text("Ragnar finds " + item(*found->item).name + ".");
                else show_text("Ragnar finds " + std::to_string(found->gold) + " gold.");
                if (result == game::InventoryResult::success) save(game.frame());
            }
        }
        else if (command == 5) { save(game.frame()); page_ = Page::field; show_text("Adventure log saved."); }
        else { save(game.frame()); page_ = Page::leaving; }
    } else if (page_ == Page::question) { page_ = Page::field; advance_interaction(cursor_ == 0); }
    else if (page_ == Page::status) page_ = Page::commands;
    else if (page_ == Page::inventory && !character.inventory.empty()) { selected_item_ = cursor_; page_ = Page::item_action; cursor_ = 0; }
    else if (page_ == Page::item_action) {
        if (cursor_ == 0) {
            const auto result = game::equip_item(adventure, member, selected_item_, items_);
            show_text(result == game::InventoryResult::success ? "Equipment changed." : result == game::InventoryResult::cursed ? "The cursed equipment cannot be removed." : "Ragnar cannot equip that item.");
        } else if (cursor_ == 1) {
            if (selected_item_ < character.inventory.size()) {
                const auto id = character.inventory[selected_item_].id;
                if (id == 83) {
                    game::Random random(game.frame().random);
                    const auto recovery = 30U + random.next_below(11);
                    character.hp = static_cast<std::uint16_t>(std::min<std::uint32_t>(character.maximum_hp, character.hp + recovery));
                    character.inventory.erase(character.inventory.begin() + selected_item_);
                    auto frame = game.frame(); frame.random = random.state(); game = game::Game(std::move(frame));
                    show_text("Ragnar recovers hit points.");
                } else if (id == 84) { character.conditions &= static_cast<std::uint16_t>(~1U); character.inventory.erase(character.inventory.begin() + selected_item_); show_text("Ragnar is cured of poison."); }
                else show_text("That item cannot be used here.");
            }
        } else {
            const auto result = game::discard_item(adventure, member, selected_item_, items_);
            show_text(result == game::InventoryResult::success ? "Item discarded." : "That item cannot be discarded.");
        }
        page_ = Page::inventory; cursor_ = 0;
    } else if (page_ == Page::selling && !character.inventory.empty()) {
        const auto& selected = item(character.inventory[cursor_].id);
        const game::ItemRule rule{selected.id, selected.power, selected.price, static_cast<game::EquipmentSlot>(selected.slot), selected.equip_mask, selected.cursed};
        const auto result = game::sell_item(adventure, member, cursor_, rule);
        show_text(result == game::InventoryResult::success ? "The item was sold." : "That item cannot be sold.");
        cursor_ = 0; save(game.frame());
    } else if (page_ == Page::merchant && !stock_.empty()) {
        const auto& selected = item(stock_.at(cursor_));
        const game::ItemRule rule{selected.id, selected.power, selected.price, static_cast<game::EquipmentSlot>(selected.slot), selected.equip_mask, selected.cursed};
        const auto result = game::buy_item(adventure, member, rule);
        show_text(result == game::InventoryResult::success ? "The item was purchased." : result == game::InventoryResult::full ? "Ragnar cannot carry any more." : "Not enough gold.");
        if (result == game::InventoryResult::success) save(game.frame());
    } else if (page_ == Page::church) {
        if (cursor_ == 0) { save(game.frame()); show_text("Adventure log saved."); }
        else if (cursor_ == 1) {
            const auto level = static_cast<std::uint32_t>(character.level);
            const auto price = (level * level + 10U) / 10U * 10U;
            if (character.hp != 0) show_text("Ragnar is already alive.");
            else if (game::spend_gold(adventure, price)) { character.hp = character.maximum_hp; character.conditions = 0; show_text("Ragnar is restored."); }
            else show_text("Not enough gold.");
        } else {
            if ((character.conditions & 1U) == 0) show_text("Ragnar is not poisoned.");
            else if (game::spend_gold(adventure, 5)) { character.conditions &= static_cast<std::uint16_t>(~1U); show_text("Ragnar is purified."); }
            else show_text("Not enough gold.");
        }
    } else if (page_ == Page::inn) {
        if (cursor_ == 0 && game::spend_gold(adventure, inn_price_)) { character.hp = character.maximum_hp; character.mp = character.maximum_mp; show_text("Ragnar is fully rested."); save(game.frame()); }
        else if (cursor_ == 0) show_text("Not enough gold.");
        else { interaction_.reset(); page_ = Page::field; }
    }
}

void GameplaySession::draw(GraphicsRenderer& renderer) const
{
    if (battle_) {
        renderer.begin({0, 0, 0, 255});
        const auto& character = field_.state().adventure.characters[field_.state().adventure.party[0]];
        view_.text(renderer, "Ragnar Lv " + std::to_string(character.level) + " HP " + std::to_string(character.hp), 16, 16);
        const auto count = battle_->monsters().size();
        for (std::size_t index = 0; index < count; ++index) if (battle_->monsters()[index].hp != 0) {
            const auto& enemy = battle_->monsters()[index];
            const auto source = std::ranges::find_if(data_.monsters(), [&](const auto& monster) { return monster.id == enemy.rule.id; });
            const int center = static_cast<int>((index + 1U) * 256U / (count + 1U));
            view_.monster(renderer, enemy.rule.id, center - source->width / 2, 136 - source->height);
        }
        if (dialogue_) {
            view_.window(renderer, 16, 144, 28, 12);
            std::size_t visible = dialogue_->visible_glyphs();
            for (std::size_t row = 0; row < dialogue_->page().lines.size(); ++row) {
                const auto& line = dialogue_->page().lines[row];
                const auto limit = std::min(visible, line.size());
                for (std::size_t column = 0; column < limit; ++column) {
                    const game::Palette colors{{{0, 0, 0, 255}, {252, 252, 252, 255}, {252, 252, 252, 255}, {252, 252, 252, 255}}};
                    renderer.draw({game::GraphicKind::font, 0, line[column], 24 + static_cast<int>(column) * 8, 152 + static_cast<int>(row) * 8, colors});
                }
                visible -= limit;
            }
        } else {
            view_.window(renderer, 16, 144, 28, 12);
            if (page_ == Page::battle) {
                const std::array<std::string_view, 3> choices{"FIGHT", "PARRY", "RUN"};
                for (std::size_t index = 0; index < choices.size(); ++index) view_.text(renderer, choices[index], 40, 168 + static_cast<int>(index) * 16);
            } else for (std::size_t index = 0; index < count; ++index) view_.text(renderer,
                std::to_string(index + 1) + " " + std::ranges::find_if(data_.monsters(), [&](const auto& monster) { return monster.id == battle_->monsters()[index].rule.id; })->name,
                40, 152 + static_cast<int>(index) * 8);
            view_.cursor(renderer, 24, (page_ == Page::battle ? 168 : 152) + cursor_ * (page_ == Page::battle ? 16 : 8));
        }
        return;
    }
    view_.draw(renderer, field_.state(), dialogue_.get());
    if (dialogue_ || page_ == Page::field || page_ == Page::leaving) return;
    const auto& adventure = field_.state().adventure;
    const auto& character = adventure.characters[adventure.party[0]];
    if (page_ == Page::status) {
        view_.window(renderer, 16, 8, 28, 29);
        view_.text(renderer, "Ragnar", 32, 32);
        const std::array<std::string, 11> rows{"Level " + std::to_string(character.level),
            "HP " + std::to_string(character.hp) + "/" + std::to_string(character.maximum_hp),
            "MP " + std::to_string(character.mp) + "/" + std::to_string(character.maximum_mp),
            "Strength " + std::to_string(character.strength), "Agility " + std::to_string(character.agility),
            "Vitality " + std::to_string(character.vitality), "Intelligence " + std::to_string(character.intelligence),
            "Luck " + std::to_string(character.luck), "Attack " + std::to_string(game::attack_power(character, items_)),
            "Defense " + std::to_string(game::defense_power(character, items_)), "EXP " + std::to_string(character.experience)};
        for (std::size_t row = 0; row < rows.size(); ++row) view_.text(renderer, rows[row], 32, 48 + static_cast<int>(row) * 16);
        view_.text(renderer, "Next EXP " + std::to_string(game::experience_to_next_level(character, curves_[character.id])), 32, 224);
        return;
    }
    std::vector<std::string> options;
    if (page_ == Page::commands) options = {"TALK", "STATUS", "ITEM", "SPELL", "SEARCH", "SAVE", "RETURN TO TITLE"};
    else if (page_ == Page::inventory || page_ == Page::selling) {
        for (const auto& value : character.inventory) options.push_back((value.equipped == game::EquipmentSlot::none ? " " : "*") + item(value.id).name);
    } else if (page_ == Page::item_action) options = {"Equip", "Use", "Discard"};
    else if (page_ == Page::question) options = {"Yes", "No"};
    else if (page_ == Page::church) options = {"Record adventure", "Restore", "Purify"};
    else if (page_ == Page::inn) options = {"Rest - " + std::to_string(inn_price_) + " gold", "No"};
    else if (page_ == Page::trade) options = {"Buy", "Sell", "Leave"};
    else if (page_ == Page::merchant) for (const auto id : stock_) options.push_back(item(id).name + " " + std::to_string(item(id).price));
    if (options.empty()) options = {"Empty"};
    view_.window(renderer, 16, 16, 28, static_cast<std::uint32_t>(options.size() * 2U + 4U));
    view_.text(renderer, "Gold " + std::to_string(adventure.gold), 32, 24);
    for (std::size_t row = 0; row < options.size(); ++row) view_.text(renderer, options[row], 40, 48 + static_cast<int>(row) * 16, 23);
    view_.cursor(renderer, 24, 48 + cursor_ * 16);
}

void GameplaySession::save(const game::FrameState& frame)
{
    if (battle_ && battle_->outcome() == game::BattleOutcome::ongoing) return;
    store_.write(slot_, snapshot(field_.state(), frame));
    last_save_ = frame.number;
}

bool GameplaySession::leaving() const noexcept { return page_ == Page::leaving; }
const game::FieldState& GameplaySession::state() const noexcept { return field_.state(); }
bool GameplaySession::dialogue_active() const noexcept { return dialogue_ != nullptr; }
bool GameplaySession::battle_active() const noexcept { return battle_ != nullptr; }

std::size_t GameplaySession::validate_actor_dialogues() const
{
    std::size_t count = 0;
    const auto context = dialogue_context();
    const auto inspect = [&](const auto& self, const std::vector<content::FieldActor::Effect>& actions) -> void {
        for (const auto& action : actions) {
            if (action.kind == 0) {
                const auto& message = catalog_.dialogue_message({action.argument});
                static_cast<void>(game::layout_dialogue(message.symbols, context, 26, 8));
                ++count;
            }
            self(self, action.yes); self(self, action.no);
        }
    };
    for (const auto& map : data_.maps()) for (const auto& actor : map.actors) {
        if (actor.interaction.empty()) throw std::runtime_error("loaded actor has no interaction program");
        inspect(inspect, actor.interaction);
    }
    const auto rules = view_.rules();
    for (const auto& map : rules) for (const auto& actor : map.actors) for (const bool answer : {false, true}) {
        auto adventure = field_.state().adventure;
        game::Interaction program(actor.interaction, actor.id, adventure);
        for (std::uint32_t actions = 0; actions < 256 && !program.finished(); ++actions) {
            const auto request = program.next(answer);
            if (request && request->kind == game::InteractionRequestKind::message) {
                const auto& message = catalog_.dialogue_message({request->value});
                static_cast<void>(game::layout_dialogue(message.symbols, context, 26, 8));
            }
        }
        if (!program.finished()) throw std::runtime_error("actor interaction exceeded smoke action bound");
    }
    return count;
}

} // namespace dw4::desktop