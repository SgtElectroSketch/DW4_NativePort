#include <dw4/game/Replay.hpp>

#include <limits>
#include <stdexcept>

namespace dw4::game {
namespace {

std::optional<ReplayDifference> compare(const FrameState& actual, const FrameState& expected)
{
    if (actual.number != expected.number) return ReplayDifference{actual.number, "frame"};
    if (actual.input != expected.input) return ReplayDifference{actual.number, "input"};
    if (actual.pressed != expected.pressed) return ReplayDifference{actual.number, "pressed"};
    if (actual.released != expected.released) return ReplayDifference{actual.number, "released"};
    if (actual.random.draw_counter != expected.random.draw_counter) return ReplayDifference{actual.number, "random.draw_counter"};
    if (actual.random.engine != expected.random.engine) return ReplayDifference{actual.number, "random.engine"};
    return std::nullopt;
}

} // namespace

ReplayResult run_replay(const Replay& replay)
{
    if (replay.frames.size() > replay_max_frames) {
        throw std::invalid_argument("replay exceeds the maximum frame count");
    }
    if (replay.initial.number > std::numeric_limits<std::uint64_t>::max() - replay.frames.size()) {
        throw std::invalid_argument("replay frame number would overflow");
    }
    std::uint64_t total_draws = 0;
    for (const auto& frame : replay.frames) {
        total_draws += frame.random_draws;
        if (total_draws > replay_max_random_draws) {
            throw std::invalid_argument("replay exceeds the maximum random draw count");
        }
    }
    if (replay.initial.random.draw_counter > std::numeric_limits<std::uint64_t>::max() - total_draws) {
        throw std::invalid_argument("replay random draw counter would overflow");
    }
    for (std::size_t index = 0; index < replay.checkpoints.size(); ++index) {
        const auto frame_index = replay.checkpoints[index].frame_index;
        if (frame_index >= replay.frames.size() ||
            (index > 0 && frame_index <= replay.checkpoints[index - 1].frame_index)) {
            throw std::invalid_argument("replay checkpoints must be in range and strictly increasing");
        }
    }

    Game game(replay.initial);
    std::size_t checkpoint_index = 0;
    for (std::size_t frame_index = 0; frame_index < replay.frames.size(); ++frame_index) {
        const auto& frame = replay.frames[frame_index];
        game.tick(frame.input);
        for (std::uint32_t draw = 0; draw < frame.random_draws; ++draw) {
            static_cast<void>(game.next_random_byte());
        }
        if (checkpoint_index < replay.checkpoints.size() && replay.checkpoints[checkpoint_index].frame_index == frame_index) {
            const auto difference = compare(game.frame(), replay.checkpoints[checkpoint_index].expected);
            if (difference) {
                return {game.frame(), difference};
            }
            ++checkpoint_index;
        }
    }
    return {game.frame(), std::nullopt};
}

} // namespace dw4::game