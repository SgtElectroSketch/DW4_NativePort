#include <dw4/game/Game.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("game advances exactly one deterministic frame per tick")
{
    dw4::game::Game game;
    CHECK(game.frame().number == 0);

    game.tick({static_cast<std::uint8_t>(dw4::game::Button::start)});

    CHECK(game.frame().number == 1);
    CHECK(game.frame().input.is_pressed(dw4::game::Button::start));
    CHECK_FALSE(game.frame().input.is_pressed(dw4::game::Button::a));
}