<!-- BEGIN AI-GENERATED CONTENT -->

# GameTests.cpp

**Source:** `tests\src\unit\GameTests.cpp`

```cpp
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
```

## Unit-Test Structure

```cpp
#include <dw4/game/Game.hpp>

#include <catch2/catch_test_macros.hpp>
```

- The test includes the same public game header used by production consumers. It does not reach into private members.
- Catch2 supplies `TEST_CASE`, `CHECK`, and `CHECK_FALSE` macros. The separately linked Catch2 main library provides the
    executable entry point that discovers and runs registered tests.
- There is no SDL include, confirming that deterministic game logic can be tested without opening a window or audio
    device.

## Arrange, Act, and Assert

```cpp
TEST_CASE("game advances exactly one deterministic frame per tick")
{
        dw4::game::Game game;
        CHECK(game.frame().number == 0);

        game.tick({static_cast<std::uint8_t>(dw4::game::Button::start)});

        CHECK(game.frame().number == 1);
        CHECK(game.frame().input.is_pressed(dw4::game::Button::start));
        CHECK_FALSE(game.frame().input.is_pressed(dw4::game::Button::a));
}
```

- **Arrange:** default-construct `Game` and verify its observable initial frame number is zero.
- **Act:** construct an `InputFrame` from the Start-button bit and advance exactly one tick.
- **Assert:** verify one frame elapsed, Start is present, and an unrelated A-button bit is absent.
- Braced construction initializes the first `InputFrame` member, `buttons`, with the cast byte.
- Catch2 `CHECK` records a failure but allows the remainder of the test case to run, potentially reporting several
    related failures in one execution.
- This test covers initial state, one tick, positive bit lookup, and negative bit lookup. It does not yet cover multiple
    simultaneous buttons, many ticks, frame-counter limits, transition input, or any real gameplay rule.

<!-- END AI-GENERATED CONTENT -->
