#include <dw4/game/Title.hpp>

#include <stdexcept>

namespace dw4::game {

Title::Title(const TitleTiming timing)
    : timing_(timing)
{
    if (timing.artwork_frame == 0 || timing.fade_frames == 0 || timing.fade_frames > 120) {
        throw std::invalid_argument("title timing must contain a bounded opening and fade duration");
    }
}

void Title::tick(const InputFrame input) noexcept
{
    if (state_.phase == TitlePhase::finished) {
        return;
    }
    ++state_.frame;
    switch (state_.phase) {
    case TitlePhase::opening:
        if (state_.frame >= timing_.artwork_frame) {
            state_.phase = TitlePhase::artwork;
        }
        break;
    case TitlePhase::artwork:
        if (input.buttons != 0) {
            state_.phase = TitlePhase::fade_out;
            state_.fade_frame = 0;
        }
        break;
    case TitlePhase::fade_out:
        ++state_.fade_frame;
        if (state_.fade_frame >= timing_.fade_frames) {
            state_.phase = TitlePhase::finished;
        }
        break;
    case TitlePhase::finished:
        break;
    }
}

const TitleState& Title::state() const noexcept
{
    return state_;
}

} // namespace dw4::game