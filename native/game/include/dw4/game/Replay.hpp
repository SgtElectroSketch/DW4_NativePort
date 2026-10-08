#pragma once

#include <dw4/game/Game.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace dw4::game {

inline constexpr std::uint32_t replay_schema_version = 1;
inline constexpr std::uint32_t replay_max_frames = 360000;
inline constexpr std::uint32_t replay_max_random_draws = 1000000;

struct ReplayFrame final {
    InputFrame input;
    std::uint32_t random_draws{};
};

struct ReplayCheckpoint final {
    std::uint32_t frame_index{};
    FrameState expected;
};

struct Replay final {
    FrameState initial;
    std::vector<ReplayFrame> frames;
    std::vector<ReplayCheckpoint> checkpoints;
};

struct ReplayDifference final {
    std::uint64_t frame{};
    std::string field;
};

struct ReplayResult final {
    FrameState final_state;
    std::optional<ReplayDifference> difference;
};

[[nodiscard]] ReplayResult run_replay(const Replay& replay);

} // namespace dw4::game