#pragma once

#include <dw4/game/Random.hpp>

#include <cstdint>

namespace dw4::game {

inline constexpr std::uint32_t logical_width = 256;
inline constexpr std::uint32_t logical_height = 240;
inline constexpr double frames_per_second = 60.0988;

enum class Button : std::uint8_t {
    a = 1U << 0U,
    b = 1U << 1U,
    select = 1U << 2U,
    start = 1U << 3U,
    up = 1U << 4U,
    down = 1U << 5U,
    left = 1U << 6U,
    right = 1U << 7U,
};

struct InputFrame final {
    std::uint8_t buttons{};

    [[nodiscard]] bool is_pressed(Button button) const noexcept;
    [[nodiscard]] constexpr bool operator==(const InputFrame&) const noexcept = default;
};

struct FrameState final {
    std::uint64_t number{};
    InputFrame input{};
    InputFrame pressed{};
    InputFrame released{};
    RandomState random{};

    [[nodiscard]] bool operator==(const FrameState&) const noexcept = default;
};

class Game final {
public:
    Game() noexcept = default;
    explicit Game(FrameState initial) noexcept;

    void tick(InputFrame input) noexcept;
    [[nodiscard]] std::uint8_t next_random_byte() noexcept;
    [[nodiscard]] const FrameState& frame() const noexcept;

private:
    FrameState frame_{};
};

} // namespace dw4::game