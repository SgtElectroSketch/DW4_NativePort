#include <dw4/game/Game.hpp>
#include <dw4/game/Random.hpp>
#include <dw4/game/Replay.hpp>
#include <dw4/game/Render.hpp>
#include <dw4/game/Title.hpp>
#include <dw4/game/MenuInput.hpp>
#include <dw4/game/AdventureLog.hpp>
#include <dw4/game/World.hpp>
#include <dw4/game/Dialogue.hpp>
#include <dw4/game/Progression.hpp>
#include <dw4/game/Field.hpp>
#include <dw4/game/Interaction.hpp>
#include <dw4/game/Inventory.hpp>
#include <dw4/game/Battle.hpp>
#include <dw4/game/Encounters.hpp>

#include <catch2/generators/catch_generators.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <algorithm>
#include <stdexcept>

TEST_CASE("game advances exactly one deterministic frame per tick")
{
    dw4::game::Game game;
    CHECK(game.frame().number == 0);

    game.tick({static_cast<std::uint8_t>(dw4::game::Button::start)});

    CHECK(game.frame().number == 1);
    CHECK(game.frame().input.is_pressed(dw4::game::Button::start));
    CHECK_FALSE(game.frame().input.is_pressed(dw4::game::Button::a));
}

TEST_CASE("chapter starting positions follow the consumed ASM start records")
{
    const std::array<dw4::game::MapPosition, 5> expected{{{2, 1, 16, 21}, {1, 1, 9, 8}, {22, 1, 27, 5},
                                                        {21, 3, 12, 15}, {20, 0, 5, 17}}};
    for (std::uint8_t chapter = 1; chapter <= expected.size(); ++chapter) {
        CHECK(dw4::game::chapter_start_position(chapter) == expected[chapter - 1U]);
    }
    CHECK_THROWS_AS(dw4::game::chapter_start_position(0), std::out_of_range);
    CHECK_THROWS_AS(dw4::game::chapter_start_position(6), std::out_of_range);
}

TEST_CASE("chapter one adventure initialization matches the consumed roster and character tables")
{
    const auto state = dw4::game::new_adventure({37, 38, 39}, 1, 7);
    CHECK(state.hero_name == std::vector<std::uint16_t>{37, 38, 39});
    CHECK(state.hero_gender == 1);
    CHECK(state.message_speed == 7);
    CHECK(state.chapter == 1);
    CHECK(state.position == dw4::game::MapPosition{2, 1, 16, 21});
    CHECK(state.party == std::array<std::uint8_t, 4>{6, 255, 255, 255});
    CHECK(state.gold == 50);
    CHECK(state.story_flags == std::vector<std::uint16_t>{81, 86});
    CHECK(state.bag.empty());
    for (std::uint8_t id = 0; id < state.characters.size(); ++id) {
        CHECK(state.characters[id].id == id);
        CHECK(state.characters[id].level == 1);
        CHECK(state.characters[id].experience == 0);
    }
    const auto& ragnar = state.characters[6];
    CHECK(ragnar.hp == 27); CHECK(ragnar.maximum_hp == 27);
    CHECK(ragnar.mp == 0); CHECK(ragnar.maximum_mp == 0);
    CHECK(ragnar.strength == 7); CHECK(ragnar.agility == 4);
    CHECK(ragnar.vitality == 13); CHECK(ragnar.intelligence == 1); CHECK(ragnar.luck == 2);
    CHECK(ragnar.inventory == std::vector<dw4::game::InventoryItem>{{2, dw4::game::EquipmentSlot::weapon},
                                                                   {38, dw4::game::EquipmentSlot::body}});
    CHECK_THROWS_AS(dw4::game::new_adventure({}, 0, 3), std::invalid_argument);
    CHECK_THROWS_AS(dw4::game::new_adventure({0}, 0, 3), std::invalid_argument);
    CHECK_THROWS_AS(dw4::game::new_adventure({141}, 0, 3), std::invalid_argument);
    CHECK_THROWS_AS(dw4::game::new_adventure({37}, 2, 3), std::invalid_argument);
    CHECK_THROWS_AS(dw4::game::new_adventure({37}, 0, 8), std::invalid_argument);
}

TEST_CASE("dialogue wraps words preserves explicit prompts and substitutes character values")
{
    dw4::game::DialogueContext context;
    context.character_names[6] = {54, 11, 17, 24, 11, 28};
    context.number = 12;
    const auto pages = dw4::game::layout_dialogue(std::array<std::uint8_t, 11>{37, 38, 0, 39, 40, 0x44, 0x49, 6, 0, 0x47, 0x46}, context, 4, 2);
    REQUIRE(pages.size() == 3);
    CHECK(pages[0].lines == std::vector<std::vector<std::uint16_t>>{{37, 38}, {39, 40}});
    CHECK(pages[1].lines == std::vector<std::vector<std::uint16_t>>{{54, 11, 17, 24}, {11, 28}});
    CHECK(pages[2].lines == std::vector<std::vector<std::uint16_t>>{{2, 3}});
    CHECK_THROWS_AS(dw4::game::layout_dialogue(std::array<std::uint8_t, 1>{0x49}), std::invalid_argument);
    CHECK_THROWS_AS(dw4::game::layout_dialogue(std::array<std::uint8_t, 2>{0x48, 3}), std::invalid_argument);
    CHECK_THROWS_AS(dw4::game::layout_dialogue({}, {}, 0, 8), std::invalid_argument);
}

TEST_CASE("dialogue page prompts require separate fresh input after revealing text")
{
    const auto pages = dw4::game::layout_dialogue(std::array<std::uint8_t, 5>{37, 38, 0x44, 39, 40});
    dw4::game::Dialogue dialogue(pages, 3);
    dialogue.tick({});
    CHECK(dialogue.visible_glyphs() == 1);
    dialogue.tick({static_cast<std::uint8_t>(dw4::game::Button::a)});
    CHECK(dialogue.waiting());
    CHECK(dialogue.page_index() == 0);
    dialogue.tick({});
    CHECK(dialogue.page_index() == 0);
    dialogue.tick({static_cast<std::uint8_t>(dw4::game::Button::a)});
    CHECK(dialogue.page_index() == 1);
    dialogue.tick({static_cast<std::uint8_t>(dw4::game::Button::b)});
    CHECK(dialogue.waiting());
    dialogue.tick({static_cast<std::uint8_t>(dw4::game::Button::b)});
    CHECK(dialogue.finished());
    dw4::game::Dialogue manual(pages, 7);
    CHECK(manual.waiting());
    for (int frame = 0; frame < 60; ++frame) manual.tick({});
    CHECK(manual.page_index() == 0);
}

TEST_CASE("progression applies real early thresholds and every crossed level without healing damage")
{
    auto adventure = dw4::game::new_adventure({37}, 0, 3);
    auto& ragnar = adventure.characters[6];
    dw4::game::LevelCurve curve;
    curve.character = 6;
    for (std::size_t level = 1; level < curve.levels.size(); ++level) {
        curve.levels[level].experience = static_cast<std::uint32_t>(level * 1000);
        curve.levels[level].targets = {10, 5, 15, 1, 2, 30, 0};
        curve.levels[level].gains = {3, 1, 2, 0, 0, 4, 0};
    }
    curve.levels[1].experience = 12; curve.levels[2].experience = 36;
    curve.levels[3].experience = 84; curve.levels[4].experience = 156;
    dw4::game::Random random({std::mt19937{73}, 0});
    CHECK(dw4::game::experience_to_next_level(ragnar, curve) == 12);
    CHECK(dw4::game::award_experience(ragnar, curve, 11, random).empty());
    CHECK(ragnar.level == 1);
    const auto raised = dw4::game::award_experience(ragnar, curve, 73, random);
    CHECK(raised.size() == 3);
    CHECK(ragnar.level == 4); CHECK(ragnar.experience == 84);
    CHECK(ragnar.strength > 7); CHECK(ragnar.vitality > 13);
    CHECK(ragnar.maximum_hp >= 27); CHECK(ragnar.hp == 27);
    CHECK(ragnar.maximum_mp == 0); CHECK(ragnar.mp == 0);
    CHECK(dw4::game::experience_to_next_level(ragnar, curve) == 72);
    ragnar.level = 99;
    CHECK(dw4::game::award_experience(ragnar, curve, 9999999, random).empty());
    CHECK(ragnar.experience == 9999999);
    CHECK(dw4::game::experience_to_next_level(ragnar, curve) == 0);
    adventure.gold = 50;
    CHECK_FALSE(dw4::game::spend_gold(adventure, 51)); CHECK(adventure.gold == 50);
    CHECK(dw4::game::spend_gold(adventure, 10)); CHECK(adventure.gold == 40);
    dw4::game::gain_gold(adventure, 9999999); CHECK(adventure.gold == 9999999);
}

TEST_CASE("field movement respects terrain actors smooth steps and map connections")
{
    dw4::game::TerrainMap castle;
    castle.id = 2; castle.submap = 1; castle.width = 4; castle.height = 4;
    castle.cells.resize(16);
    castle.behaviors[1] = 128;
    castle.cells[1] = 1;
    castle.actors.push_back({7, 17, 2, 1, dw4::game::Direction::down});
    castle.connections.push_back({1, 2, {255, 0, 2, 2}});
    dw4::game::TerrainMap world;
    world.id = 255; world.width = 4; world.height = 4; world.cells.resize(16);
    dw4::game::FieldState state;
    state.adventure = dw4::game::new_adventure({37}, 0, 3);
    state.adventure.position = {2, 1, 1, 1};
    dw4::game::Field field({castle, world}, state);
    field.tick({static_cast<std::uint8_t>(dw4::game::Button::up)});
    CHECK(field.state().adventure.position.y == 1);
    field.tick({static_cast<std::uint8_t>(dw4::game::Button::right)});
    CHECK(field.actor_in_front() == 7);
    CHECK(field.state().adventure.position.x == 1);
    field.tick({static_cast<std::uint8_t>(dw4::game::Button::down)});
    CHECK(field.state().player_offset_y == -16);
    CHECK(field.state().adventure.position.y == 2);
    for (int frame = 0; frame < 7; ++frame) field.tick({});
    CHECK(field.state().adventure.position.map == 2);
    field.tick({});
    CHECK(field.state().adventure.position == dw4::game::MapPosition{255, 0, 2, 2});
    CHECK(field.state().player_offset_y == 0);
    CHECK(field.state().actors.empty());
    field.set_flag(28, true); CHECK(field.flag(28));
    field.set_flag(28, true); CHECK(field.state().adventure.story_flags.size() == 3);
    field.set_flag(28, false); CHECK_FALSE(field.flag(28));
}

TEST_CASE("actor interactions pause at dialogue and preserve yes no effect ordering")
{
    auto state = dw4::game::new_adventure({37}, 0, 3);
    state.story_flags.clear();
    dw4::game::FieldEffect ask{1, 5};
    ask.yes = {{2, 1, 3, 16}, {0, 769}};
    ask.no = {{0, 770}};
    dw4::game::Interaction interaction({{0, 768}, ask, {3, 5}}, 7, state);
    REQUIRE(interaction.next());
    CHECK(state.story_flags.empty());
    CHECK_FALSE(interaction.next());
    const auto question = interaction.next(true);
    REQUIRE(question); CHECK(question->kind == dw4::game::InteractionRequestKind::question);
    const auto answer = interaction.next(true);
    REQUIRE(answer); CHECK(answer->value == 769);
    CHECK(state.story_flags == std::vector<std::uint16_t>{28});
    const auto shop = interaction.next(true);
    REQUIRE(shop); CHECK(shop->kind == dw4::game::InteractionRequestKind::shop); CHECK(shop->value == 5);
    CHECK_FALSE(interaction.next(true)); CHECK(interaction.finished());
    dw4::game::Interaction decline({ask}, 8, state);
    REQUIRE(decline.next());
    const auto refused = decline.next(false);
    REQUIRE(refused); CHECK(refused->value == 770);
}

TEST_CASE("opening actors remain visible until their removal path finishes")
{
    dw4::game::TerrainMap map;
    map.id = 2; map.submap = 1; map.width = 4; map.height = 4; map.cells.resize(16);
    dw4::game::ActorDefinition actor{12, 24, 1, 1, dw4::game::Direction::down};
    actor.patrol = {{1, 2}}; actor.remove_at_end = true;
    map.actors.push_back(actor);
    dw4::game::FieldState state;
    state.adventure = dw4::game::new_adventure({37}, 0, 3);
    state.adventure.position = {2, 1, 3, 3}; state.phase = 1;
    dw4::game::Field field({map}, state);
    CHECK(field.state().actors[0].active);
    field.tick({}); CHECK(field.state().actors[0].active);
    field.tick_presentation_actors();
    CHECK(field.state().actors[0].active); CHECK(field.state().actors[0].step == 8);
    for (int frame = 0; frame < 8; ++frame) field.tick_presentation_actors();
    CHECK(field.state().actors[0].active);
    for (int frame = 0; frame < 10; ++frame) field.tick_presentation_actors();
    CHECK_FALSE(field.state().actors[0].active);
}

TEST_CASE("inventory transactions preserve money and equipment on failed operations")
{
    auto state = dw4::game::new_adventure({37}, 0, 3);
    const std::array<dw4::game::ItemRule, 3> items{{{2, 12, 100, dw4::game::EquipmentSlot::weapon, 103, false},
        {38, 12, 180, dw4::game::EquipmentSlot::body, 243, false}, {83, 0, 8, dw4::game::EquipmentSlot::none, 0, false}}};
    CHECK(dw4::game::attack_power(state.characters[6], items) == 19);
    CHECK(dw4::game::defense_power(state.characters[6], items) == 14);
    CHECK(dw4::game::buy_item(state, 6, items[0]) == dw4::game::InventoryResult::insufficient_gold);
    CHECK(state.gold == 50); CHECK(state.characters[6].inventory.size() == 2);
    CHECK(dw4::game::buy_item(state, 6, items[2]) == dw4::game::InventoryResult::success);
    CHECK(state.gold == 42); CHECK(state.characters[6].inventory.size() == 3);
    CHECK(dw4::game::equip_item(state, 6, 2, items) == dw4::game::InventoryResult::cannot_equip);
    CHECK(dw4::game::sell_item(state, 6, 2, items[2]) == dw4::game::InventoryResult::success);
    CHECK(state.gold == 48); CHECK(state.characters[6].inventory.size() == 2);
    state.characters[6].inventory.resize(8);
    CHECK(dw4::game::buy_item(state, 6, items[2]) == dw4::game::InventoryResult::full);
    CHECK(state.gold == 48);
}

TEST_CASE("actor flag predicates preserve the original zero success branch convention")
{
    auto state = dw4::game::new_adventure({37}, 0, 3);
    dw4::game::FieldEffect branch{1, 0x1B, 14, 16};
    branch.yes = {{0, 1929}};
    branch.no = {{0, 1927}};
    dw4::game::Interaction before({branch}, 8, state);
    const auto first = before.next(); REQUIRE(first); CHECK(first->value == 1927);
    state.story_flags.push_back(14 * 8 + 4);
    dw4::game::Interaction after({branch}, 8, state);
    const auto second = after.next(); REQUIRE(second); CHECK(second->value == 1929);
}

TEST_CASE("physical battle awards actual money experience and levels exactly once")
{
    auto state = dw4::game::new_adventure({37}, 0, 3);
    dw4::game::LevelCurve curve;
    curve.character = 6;
    for (std::size_t level = 1; level < 99; ++level) {
        curve.levels[level].experience = static_cast<std::uint32_t>(level * 1000);
        curve.levels[level].targets = {10, 5, 15, 1, 2, 30, 0}; curve.levels[level].gains = {3, 1, 2, 0, 0, 4, 0};
    }
    curve.levels[1].experience = 12;
    state.characters[6].experience = 11;
    const std::vector<dw4::game::ItemRule> items{{2, 12, 100, dw4::game::EquipmentSlot::weapon, 103}, {38, 12, 180, dw4::game::EquipmentSlot::body, 243}};
    dw4::game::MonsterRule slime{0, 8, 0, 9, 5, 3, 1, 2};
    dw4::game::Random random({std::mt19937{73}, 0});
    dw4::game::Battle battle({slime}, state, curve, items);
    for (int turn = 0; turn < 10 && battle.outcome() == dw4::game::BattleOutcome::ongoing; ++turn) {
        static_cast<void>(battle.turn(dw4::game::BattleCommand::attack, 0, random));
    }
    REQUIRE(battle.outcome() == dw4::game::BattleOutcome::victory);
    CHECK(state.gold == 52); CHECK(state.characters[6].experience == 12); CHECK(state.characters[6].level == 2);
    CHECK(battle.turn(dw4::game::BattleCommand::attack, 0, random).empty());
    CHECK(state.gold == 52);
    for (int sample = 0; sample < 64; ++sample) CHECK(dw4::game::physical_damage(19, 5, random) >= 7);
}

TEST_CASE("treasure rewards are transactional and cannot be collected twice")
{
    auto state = dw4::game::new_adventure({37}, 0, 3);
    CHECK(dw4::game::collect_treasure(state, 4096, std::nullopt, 40) == dw4::game::InventoryResult::success);
    CHECK(state.gold == 90);
    CHECK(dw4::game::collect_treasure(state, 4096, std::nullopt, 40) == dw4::game::InventoryResult::invalid_item);
    CHECK(state.gold == 90);
    state.characters[6].inventory.resize(8);
    CHECK(dw4::game::collect_treasure(state, 4097, std::uint16_t{83}, 0) == dw4::game::InventoryResult::full);
    CHECK(std::ranges::find(state.story_flags, std::uint16_t{4097}) == state.story_flags.end());
}

TEST_CASE("world encounters use terrain rates day masks and weighted formation data")
{
    dw4::game::EncounterRules rules;
    rules.grid.fill(4); rules.terrain_rates.fill(255); rules.first_steps = {2, 4, 8};
    rules.groups[4].rate = 255;
    rules.groups[4].entries.fill(255);
    rules.groups[4].entries[11] = 0;
    rules.groups[4].weights[11] = 255;
    dw4::game::Random random({std::mt19937{73}, 0});
    const auto formation = dw4::game::roll_world_encounter(rules, 164, 46, 1, 3, false, random);
    REQUIRE(formation); REQUIRE(formation->groups.size() == 1);
    CHECK(formation->groups[0].monster == 0); CHECK(formation->groups[0].count == 1);
    CHECK_FALSE(dw4::game::roll_world_encounter(rules, 164, 46, 8, 3, false, random));
}

TEST_CASE("game derives button edges without repeating held presses")
{
    const auto button = GENERATE(dw4::game::Button::a, dw4::game::Button::b, dw4::game::Button::select,
                                 dw4::game::Button::start, dw4::game::Button::up, dw4::game::Button::down,
                                 dw4::game::Button::left, dw4::game::Button::right);
    CAPTURE(button);
    const auto mask = static_cast<std::uint8_t>(button);
    dw4::game::Game game;
    CHECK(game.frame().pressed.buttons == 0);
    CHECK(game.frame().released.buttons == 0);

    game.tick({mask});
    CHECK(game.frame().input.buttons == mask);
    CHECK(game.frame().pressed.buttons == mask);
    CHECK(game.frame().released.buttons == 0);

    game.tick({mask});
    CHECK(game.frame().input.buttons == mask);
    CHECK(game.frame().pressed.buttons == 0);
    CHECK(game.frame().released.buttons == 0);

    game.tick({});
    CHECK(game.frame().input.buttons == 0);
    CHECK(game.frame().pressed.buttons == 0);
    CHECK(game.frame().released.buttons == mask);

    game.tick({});
    CHECK(game.frame().released.buttons == 0);

    game.tick({mask});
    CHECK(game.frame().pressed.buttons == mask);
    CHECK(game.frame().released.buttons == 0);
    CHECK(game.frame().number == 5);
}

TEST_CASE("game tracks simultaneous button changes independently")
{
    dw4::game::Game game;
    const auto original_buttons = static_cast<std::uint8_t>(
        static_cast<std::uint8_t>(dw4::game::Button::a) | static_cast<std::uint8_t>(dw4::game::Button::up));
    const auto changed_buttons = static_cast<std::uint8_t>(
        static_cast<std::uint8_t>(dw4::game::Button::b) | static_cast<std::uint8_t>(dw4::game::Button::up));

    game.tick({original_buttons});
    game.tick({changed_buttons});
    CHECK(game.frame().input.buttons == changed_buttons);
    CHECK(game.frame().pressed.is_pressed(dw4::game::Button::b));
    CHECK_FALSE(game.frame().pressed.is_pressed(dw4::game::Button::up));
    CHECK(game.frame().released.is_pressed(dw4::game::Button::a));
    CHECK_FALSE(game.frame().released.is_pressed(dw4::game::Button::up));

    game.tick({255});
    CHECK(game.frame().input.buttons == 255);
    CHECK(game.frame().pressed.buttons == static_cast<std::uint8_t>(255U & ~changed_buttons));
    CHECK(game.frame().released.buttons == 0);
    game.tick({});
    CHECK(game.frame().pressed.buttons == 0);
    CHECK(game.frame().released.buttons == 255);
}

TEST_CASE("identical input sequences produce identical frame snapshots")
{
    dw4::game::Game first;
    dw4::game::Game second;
    CHECK(first.frame() == second.frame());

    for (std::uint32_t frame_index = 0; frame_index < 256; ++frame_index) {
        const auto buttons = static_cast<std::uint8_t>((frame_index * 37U + (frame_index >> 2U)) & 255U);
        first.tick({buttons});
        second.tick({buttons});
        CHECK(first.frame() == second.frame());
        CHECK(first.frame().input.buttons == buttons);
    }
}

TEST_CASE("random byte draws use the standard engine and distribution")
{
    const auto seed = GENERATE(std::uint32_t{0}, std::uint32_t{1}, std::uint32_t{0x1234}, std::uint32_t{0xFFFFFFFF});
    CAPTURE(seed);
    dw4::game::Random random({std::mt19937{seed}, 17});
    std::mt19937 expected_engine(seed);
    std::uniform_int_distribution<std::uint32_t> distribution(0, 255);
    for (std::uint32_t draw = 0; draw < 256; ++draw) {
        CHECK(random.next_byte() == distribution(expected_engine));
        CHECK(random.state().engine == expected_engine);
        CHECK(random.state().draw_counter == draw + 18U);
    }
}

TEST_CASE("random bounds use standard uniform sampling")
{
    const auto upper_bound = GENERATE(std::uint32_t{1}, std::uint32_t{2}, std::uint32_t{3}, std::uint32_t{256},
                                     std::uint32_t{65535}, std::numeric_limits<std::uint32_t>::max());
    CAPTURE(upper_bound);
    dw4::game::Random random({std::mt19937{73}, 0});
    std::mt19937 expected_engine(73);
    std::uniform_int_distribution<std::uint32_t> distribution(0, upper_bound - 1U);
    for (std::uint32_t draw = 0; draw < 100; ++draw) {
        const auto value = random.next_below(upper_bound);
        CHECK(value == distribution(expected_engine));
        CHECK(value < upper_bound);
        CHECK(random.state().engine == expected_engine);
        CHECK(random.state().draw_counter == draw + 1U);
    }
}

TEST_CASE("random invalid bounds leave the engine and counter unchanged")
{
    dw4::game::Random random({std::mt19937{0x1234}, 9});
    const auto before = random.state();
    CHECK_THROWS_AS(random.next_below(0), std::invalid_argument);
    CHECK(random.state() == before);
}

TEST_CASE("restored random state continues the same sequence")
{
    dw4::game::Random original({std::mt19937{0x1234}, 73});
    for (std::uint32_t draw = 0; draw < 37; ++draw) {
        static_cast<void>(original.next_byte());
    }
    dw4::game::Random restored(original.state());
    for (std::uint32_t draw = 0; draw < 256; ++draw) {
        CHECK(original.next_byte() == restored.next_byte());
        CHECK(original.state() == restored.state());
    }
}

TEST_CASE("game snapshots own RNG state without implicit frame draws")
{
    dw4::game::FrameState initial;
    initial.number = 71;
    initial.input.buttons = static_cast<std::uint8_t>(dw4::game::Button::up);
    initial.random = {std::mt19937{0x1234}, 91};
    dw4::game::Game game(initial);
    CHECK(game.frame() == initial);

    game.tick(initial.input);
    CHECK(game.frame().random == initial.random);
    dw4::game::Random expected(initial.random);
    CHECK(game.next_random_byte() == expected.next_byte());
    CHECK(game.frame().random == expected.state());

    dw4::game::Game restored(game.frame());
    for (std::uint32_t frame = 0; frame < 100; ++frame) {
        const dw4::game::InputFrame input{static_cast<std::uint8_t>(frame * 7U)};
        game.tick(input);
        restored.tick(input);
        if (frame % 3U == 0) {
            CHECK(game.next_random_byte() == restored.next_random_byte());
        }
        CHECK(game.frame() == restored.frame());
    }
}

TEST_CASE("replays restore initial state and match explicit frame checkpoints")
{
    dw4::game::Replay replay;
    replay.initial.number = 71;
    replay.initial.random = {std::mt19937{73}, 9};
    dw4::game::Game expected(replay.initial);
    for (std::uint32_t frame_index = 0; frame_index < 100; ++frame_index) {
        dw4::game::ReplayFrame frame;
        frame.input.buttons = static_cast<std::uint8_t>(frame_index * 3U);
        frame.random_draws = frame_index % 3U;
        expected.tick(frame.input);
        for (std::uint32_t draw = 0; draw < frame.random_draws; ++draw) {
            static_cast<void>(expected.next_random_byte());
        }
        replay.frames.push_back(frame);
        if (frame_index % 10U == 0) {
            replay.checkpoints.push_back({frame_index, expected.frame()});
        }
    }
    const auto first = dw4::game::run_replay(replay);
    const auto second = dw4::game::run_replay(replay);
    CHECK_FALSE(first.difference);
    CHECK(first.final_state == expected.frame());
    CHECK(first.final_state == second.final_state);
}

TEST_CASE("replays stop at the first divergent frame and name its field")
{
    const auto field = GENERATE(0, 1, 2, 3, 4, 5);
    CAPTURE(field);
    dw4::game::Replay replay;
    dw4::game::Game expected;
    for (std::uint32_t frame_index = 0; frame_index < 5; ++frame_index) {
        expected.tick({static_cast<std::uint8_t>(frame_index)});
        replay.frames.push_back({expected.frame().input, 0});
        replay.checkpoints.push_back({frame_index, expected.frame()});
    }
    auto& checkpoint = replay.checkpoints[2].expected;
    std::string expected_field;
    switch (field) {
    case 0: ++checkpoint.number; expected_field = "frame"; break;
    case 1: checkpoint.input.buttons ^= 1U; expected_field = "input"; break;
    case 2: checkpoint.pressed.buttons ^= 1U; expected_field = "pressed"; break;
    case 3: checkpoint.released.buttons ^= 1U; expected_field = "released"; break;
    case 4: ++checkpoint.random.draw_counter; expected_field = "random.draw_counter"; break;
    case 5: static_cast<void>(checkpoint.random.engine()); expected_field = "random.engine"; break;
    default: FAIL("invalid fixture field");
    }
    const auto result = dw4::game::run_replay(replay);
    REQUIRE(result.difference);
    CHECK(result.difference->frame == 3);
    CHECK(result.difference->field == expected_field);
    CHECK(result.final_state.number == 3);
}

TEST_CASE("replays reject overflow and excessive work before execution")
{
    dw4::game::Replay replay;
    replay.initial.number = std::numeric_limits<std::uint64_t>::max();
    replay.frames.push_back({{}, 0});
    CHECK_THROWS_AS(dw4::game::run_replay(replay), std::invalid_argument);

    replay.initial.number = 0;
    replay.frames[0].random_draws = dw4::game::replay_max_random_draws + 1U;
    CHECK_THROWS_AS(dw4::game::run_replay(replay), std::invalid_argument);

    replay.frames[0].random_draws = 1;
    replay.initial.random.draw_counter = std::numeric_limits<std::uint64_t>::max();
    CHECK_THROWS_AS(dw4::game::run_replay(replay), std::invalid_argument);

    replay.initial = {};
    replay.frames.assign(dw4::game::replay_max_frames + 1U, {});
    CHECK_THROWS_AS(dw4::game::run_replay(replay), std::invalid_argument);
}

TEST_CASE("replays validate sparse checkpoint ordering and indexes")
{
    dw4::game::Replay replay;
    replay.frames.assign(3, {});
    replay.checkpoints = {{3, {}}};
    CHECK_THROWS_AS(dw4::game::run_replay(replay), std::invalid_argument);
    replay.checkpoints = {{1, {}}, {1, {}}};
    CHECK_THROWS_AS(dw4::game::run_replay(replay), std::invalid_argument);
    replay.checkpoints = {{2, {}}, {1, {}}};
    CHECK_THROWS_AS(dw4::game::run_replay(replay), std::invalid_argument);
    replay.checkpoints.clear();
    CHECK_FALSE(dw4::game::run_replay(replay).difference);
    CHECK(sizeof(dw4::game::ReplayFrame) <= 16);
}

TEST_CASE("render layouts produce stable metatile quadrant commands")
{
    const auto flip_horizontal = GENERATE(false, true);
    const auto flip_vertical = GENERATE(false, true);
    const std::array<std::uint16_t, 4> tiles{1, 2, 3, 4};
    dw4::game::TileDraw origin;
    origin.x = 16;
    origin.y = 24;
    origin.flip_horizontal = flip_horizontal;
    origin.flip_vertical = flip_vertical;
    std::vector<dw4::game::TileDraw> commands;
    dw4::game::append_metatile(commands, origin, tiles);
    REQUIRE(commands.size() == 4);
    for (std::uint32_t index = 0; index < 4; ++index) {
        const auto column = index % 2U;
        const auto row = index / 2U;
        CHECK(commands[index].tile == tiles[(flip_vertical ? 1U - row : row) * 2U + (flip_horizontal ? 1U - column : column)]);
        CHECK(commands[index].x == 16 + static_cast<std::int32_t>(column * 8U));
        CHECK(commands[index].y == 24 + static_cast<std::int32_t>(row * 8U));
        CHECK(commands[index].flip_horizontal == flip_horizontal);
        CHECK(commands[index].flip_vertical == flip_vertical);
    }
}

TEST_CASE("render layouts bound windows and glyph grids before mutation")
{
    std::vector<dw4::game::TileDraw> commands;
    dw4::game::TileDraw origin;
    const dw4::game::WindowTiles border{1, 2, 3, 4, 5, 6, 7, 8, 9};
    dw4::game::append_window(commands, origin, 3, 3, border);
    REQUIRE(commands.size() == 9);
    for (std::size_t index = 0; index < 9; ++index) {
        CHECK(commands[index].tile == index + 1);
    }
    CHECK_THROWS_AS(dw4::game::append_window(commands, origin, 2, 3, border), std::invalid_argument);
    CHECK(commands.size() == 9);
    const std::array<std::uint16_t, 3> glyphs{37, 38, 39};
    dw4::game::append_glyphs(commands, origin, glyphs, 2, 2);
    CHECK(commands[9].tile == 37);
    CHECK(commands[10].x == 8);
    CHECK(commands[11].x == 0);
    CHECK(commands[11].y == 8);
    CHECK_THROWS_AS(dw4::game::append_glyphs(commands, origin, glyphs, 1, 1), std::invalid_argument);
    origin.x = std::numeric_limits<std::int32_t>::max();
    CHECK_THROWS_AS(dw4::game::append_metatile(commands, origin, {1, 2, 3, 4}), std::invalid_argument);
    origin.x = -7;
    origin.y = -7;
    const auto previous_count = commands.size();
    dw4::game::append_metatile(commands, origin, {1, 2, 3, 4});
    CHECK(commands[previous_count].x == -7);
    CHECK(commands[previous_count + 3].x == 1);
    CHECK(commands[previous_count + 3].y == 1);
}

TEST_CASE("title scene advances explicit phases and accepts existing normalized buttons")
{
    const auto button = GENERATE(std::uint8_t{1}, std::uint8_t{2}, std::uint8_t{4}, std::uint8_t{8},
                                 std::uint8_t{16}, std::uint8_t{32}, std::uint8_t{64}, std::uint8_t{128});
    dw4::game::Title title({3, 4});
    title.tick({button});
    CHECK(title.state().phase == dw4::game::TitlePhase::opening);
    title.tick({});
    title.tick({});
    CHECK(title.state().phase == dw4::game::TitlePhase::artwork);
    title.tick({});
    CHECK(title.state().phase == dw4::game::TitlePhase::artwork);
    title.tick({button});
    CHECK(title.state().phase == dw4::game::TitlePhase::fade_out);
    for (std::uint32_t frame = 0; frame < 3; ++frame) {
        title.tick({button});
        CHECK(title.state().phase == dw4::game::TitlePhase::fade_out);
    }
    title.tick({});
    CHECK(title.state().phase == dw4::game::TitlePhase::finished);
    const auto finished = title.state();
    title.tick({button});
    CHECK(title.state() == finished);
}

TEST_CASE("title scene rejects invalid timing and repeats deterministically")
{
    CHECK_THROWS_AS(dw4::game::Title(dw4::game::TitleTiming{0, 1}), std::invalid_argument);
    CHECK_THROWS_AS(dw4::game::Title(dw4::game::TitleTiming{1, 0}), std::invalid_argument);
    CHECK_THROWS_AS(dw4::game::Title(dw4::game::TitleTiming{1, 121}), std::invalid_argument);
    dw4::game::Title first({5, 24});
    dw4::game::Title second({5, 24});
    for (std::uint32_t frame = 0; frame < 100; ++frame) {
        const dw4::game::InputFrame input{frame == 30 ? std::uint8_t{8} : std::uint8_t{0}};
        first.tick(input);
        second.tick(input);
        CHECK(first.state() == second.state());
    }
    dw4::game::Title waiting({364, 25});
    for (std::uint32_t frame = 0; frame < 10000; ++frame) {
        waiting.tick({});
    }
    CHECK(waiting.state().phase == dw4::game::TitlePhase::artwork);
    CHECK(waiting.state().frame == 10000);
}

TEST_CASE("menu input uses press edges and explicit deterministic repeat")
{
    dw4::game::MenuInput input({3, 2});
    const auto held = static_cast<std::uint8_t>(static_cast<std::uint8_t>(dw4::game::Button::a) |
                                               static_cast<std::uint8_t>(dw4::game::Button::down));
    auto actions = input.tick({held});
    CHECK(actions.confirm);
    CHECK(actions.down);
    CHECK_FALSE(input.tick({held}).confirm);
    CHECK_FALSE(input.tick({held}).down);
    CHECK(input.tick({held}).down);
    CHECK_FALSE(input.tick({held}).down);
    CHECK(input.tick({held}).down);
    input.reset({held});
    CHECK_FALSE(input.tick({held}).confirm);
    static_cast<void>(input.tick({}));
    CHECK(input.tick({held}).confirm);
    CHECK_THROWS_AS(dw4::game::MenuInput(dw4::game::MenuRepeat{0, 1}), std::invalid_argument);
}

TEST_CASE("menu input cancels opposing directions and preserves cancel edges")
{
    dw4::game::MenuInput input;
    const auto actions = input.tick({255});
    CHECK_FALSE(actions.up);
    CHECK_FALSE(actions.down);
    CHECK_FALSE(actions.left);
    CHECK_FALSE(actions.right);
    CHECK(actions.confirm);
    CHECK(actions.cancel);
    CHECK_FALSE(input.tick({255}).cancel);
}

TEST_CASE("adventure log creates a bounded name and waits for persistence completion")
{
    dw4::game::AdventureLog menu({}, {37, 38, 39});
    REQUIRE(menu.commands().size() == 1);
    menu.tick({.confirm = true});
    CHECK(menu.state().page == dw4::game::LogPage::select_slot);
    menu.tick({.confirm = true});
    CHECK(menu.state().page == dw4::game::LogPage::name);
    for (std::uint32_t character = 0; character < 10; ++character) menu.tick({.confirm = true});
    CHECK(menu.state().name.size() == 8);
    menu.tick({.cancel = true});
    CHECK(menu.state().name.size() == 7);
    for (std::uint32_t move = 0; move < 4; ++move) menu.tick({.right = true});
    menu.tick({.confirm = true});
    CHECK(menu.state().page == dw4::game::LogPage::gender);
    menu.tick({.confirm = true});
    menu.tick({.confirm = true});
    menu.tick({.confirm = true});
    const auto request = menu.take_request();
    REQUIRE(request);
    CHECK(request->operation == dw4::game::LogOperation::create);
    CHECK(request->name.size() == 7);
    menu.tick({.cancel = true});
    CHECK(menu.state().page == dw4::game::LogPage::confirmation);
    menu.complete({}, "Disk write failed.");
    CHECK(menu.state().message == "Disk write failed.");
}

TEST_CASE("adventure log exposes only available slot operations and cancels transactions")
{
    std::array<dw4::game::LogSlot, 3> slots{};
    slots[0].occupied = true;
    slots[0].name = {37};
    dw4::game::AdventureLog menu(slots, {37, 38});
    CHECK(menu.commands().size() == 5);
    CHECK(menu.commands() == std::vector{dw4::game::LogOperation::load, dw4::game::LogOperation::speed,
                                       dw4::game::LogOperation::create, dw4::game::LogOperation::copy, dw4::game::LogOperation::erase});
    menu.tick({.down = true});
    menu.tick({.down = true});
    menu.tick({.down = true});
    menu.tick({.confirm = true});
    CHECK(menu.state().operation == dw4::game::LogOperation::copy);
    menu.tick({.confirm = true});
    CHECK(menu.state().page == dw4::game::LogPage::select_destination);
    menu.tick({.cancel = true});
    CHECK(menu.state().page == dw4::game::LogPage::select_slot);
    CHECK(menu.state().cursor == 0);
    menu.tick({.cancel = true});
    CHECK(menu.state().page == dw4::game::LogPage::commands);
    CHECK(menu.state().cursor == 3);
    CHECK_FALSE(menu.take_request());
    CHECK(menu.slots()[0].occupied);
    menu.tick({.cancel = true});
    CHECK(menu.state().page == dw4::game::LogPage::return_to_title);
}

TEST_CASE("adventure log erase and speed changes require confirmed requests")
{
    std::array<dw4::game::LogSlot, 3> slots{};
    slots[0].occupied = true;
    slots[0].name = {37};
    dw4::game::AdventureLog erase(slots, {37, 38}, false);
    CHECK(erase.commands().size() == 4);
    erase.tick({.down = true}); erase.tick({.down = true});
    erase.tick({.down = true});
    erase.tick({.confirm = true}); erase.tick({.confirm = true});
    CHECK(erase.state().page == dw4::game::LogPage::confirmation);
    erase.tick({.down = true}); erase.tick({.confirm = true});
    CHECK_FALSE(erase.take_request());
    CHECK(erase.slots()[0].occupied);
    CHECK(erase.state().page == dw4::game::LogPage::select_slot);
    erase.tick({.confirm = true}); erase.tick({.confirm = true});
    const auto request = erase.take_request();
    REQUIRE(request);
    CHECK(request->operation == dw4::game::LogOperation::erase);
    CHECK(request->source == 0);
    auto empty = slots;
    empty[0] = {};
    erase.complete(empty);
    CHECK_FALSE(erase.slots()[0].occupied);

    dw4::game::AdventureLog speed(slots, {37, 38}, false);
    speed.tick({.confirm = true}); speed.tick({.confirm = true});
    CHECK(speed.state().page == dw4::game::LogPage::speed);
    speed.tick({.down = true}); speed.tick({.confirm = true}); speed.tick({.confirm = true});
    const auto change = speed.take_request();
    REQUIRE(change);
    CHECK(change->operation == dw4::game::LogOperation::speed);
    CHECK(change->speed == 4);
}

TEST_CASE("message speed follows the verified assembly delay order and manual mode")
{
    const std::array<std::uint8_t, 7> expected{15, 21, 27, 33, 40, 46, 54};
    for (std::uint8_t speed = 0; speed < expected.size(); ++speed) {
        REQUIRE(dw4::game::message_speed_delay(speed));
        CHECK(*dw4::game::message_speed_delay(speed) == expected[speed]);
    }
    CHECK_FALSE(dw4::game::message_speed_delay(7));
    CHECK_THROWS_AS(dw4::game::message_speed_delay(8), std::out_of_range);
}