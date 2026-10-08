#pragma once

#include <dw4/game/Game.hpp>

#include <array>
#include <cstdint>

namespace dw4::game {

struct MenuActions final {
    bool up{};
    bool down{};
    bool left{};
    bool right{};
    bool confirm{};
    bool cancel{};
};

struct MenuRepeat final {
    std::uint16_t delay{18};
    std::uint16_t interval{6};
};

class MenuInput final {
public:
    explicit MenuInput(MenuRepeat repeat = {});
    [[nodiscard]] MenuActions tick(InputFrame input) noexcept;
    void reset(InputFrame held = {}) noexcept;

private:
    MenuRepeat repeat_;
    std::uint8_t previous_{};
    std::array<std::uint16_t, 4> held_frames_{};
};

} // namespace dw4::game