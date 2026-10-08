#pragma once

#include <dw4/game/Game.hpp>

#include <cstdint>

namespace dw4::game {

enum class TitlePhase : std::uint8_t {
    opening,
    artwork,
    fade_out,
    finished,
};

struct TitleTiming final {
    std::uint32_t artwork_frame{};
    std::uint32_t fade_frames{24};
};

struct TitleState final {
    std::uint64_t frame{};
    TitlePhase phase{TitlePhase::opening};
    std::uint32_t fade_frame{};

    [[nodiscard]] constexpr bool operator==(const TitleState&) const noexcept = default;
};

class Title final {
public:
    explicit Title(TitleTiming timing);
    void tick(InputFrame input) noexcept;
    [[nodiscard]] const TitleState& state() const noexcept;

private:
    TitleTiming timing_;
    TitleState state_;
};

} // namespace dw4::game