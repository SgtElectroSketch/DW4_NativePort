#include <dw4/game/Interaction.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace dw4::game {

Interaction::Interaction(std::vector<FieldEffect> program, const std::uint16_t actor, AdventureState& adventure)
    : program_(std::move(program)), actor_(actor), adventure_(adventure)
{
    stack_.push_back({&program_, 0});
}

std::uint8_t Interaction::flag_byte(const std::uint16_t index) const
{
    std::uint8_t value = 0;
    for (std::uint16_t bit = 0; bit < 8; ++bit) {
        const auto identity = static_cast<std::uint16_t>(index * 8U + bit);
        if (std::ranges::find(adventure_.story_flags, identity) != adventure_.story_flags.end()) value |= static_cast<std::uint8_t>(1U << bit);
    }
    return value;
}

void Interaction::flags(const FieldEffect& action)
{
    const auto previous = flag_byte(action.value);
    const auto mask = static_cast<std::uint8_t>(action.extra);
    const auto value = action.argument == 0 ? previous & mask : action.argument == 1 ? previous | mask : mask;
    for (std::uint16_t bit = 0; bit < 8; ++bit) {
        const auto identity = static_cast<std::uint16_t>(action.value * 8U + bit);
        const auto found = std::ranges::find(adventure_.story_flags, identity);
        if ((value & (1U << bit)) != 0 && found == adventure_.story_flags.end()) adventure_.story_flags.push_back(identity);
        else if ((value & (1U << bit)) == 0 && found != adventure_.story_flags.end()) adventure_.story_flags.erase(found);
    }
}

bool Interaction::condition(const FieldEffect& action) const
{
    bool result;
    switch (action.argument) {
    case 2: result = true; break;
    case 3: result = std::ranges::find(adventure_.party, static_cast<std::uint8_t>(action.value)) != adventure_.party.end(); break;
    case 0x11:
        result = std::ranges::any_of(adventure_.characters, [&](const auto& character) {
            return std::ranges::any_of(character.inventory, [&](const auto& item) { return item.id == action.value; });
        });
        break;
    case 0x13: result = std::ranges::find(adventure_.party, static_cast<std::uint8_t>(action.value)) != adventure_.party.end() && adventure_.characters.at(action.value).hp > 0; break;
    case 0x14: result = true; break;
    case 0x16: result = adventure_.chapter == action.value; break;
    case 0x1A: result = flag_byte(action.value) != action.extra; break;
    case 0x1B: result = (flag_byte(action.value) & action.extra) != 0; break;
    case 0x1C: result = (flag_byte(action.value) | action.extra) != 0; break;
    case 0x1E: result = adventure_.characters[adventure_.party[0]].conditions == 0; break;
    default: throw std::invalid_argument("interaction condition requires reviewed native semantics");
    }
    return action.invert ? !result : result;
}

std::optional<InteractionRequest> Interaction::next(const std::optional<bool> response)
{
    if (finished_) return std::nullopt;
    if (waiting_) {
        if (!response) return std::nullopt;
        waiting_ = false;
        if (pending_question_ != nullptr) {
            const bool answer = pending_question_->invert ? !*response : *response;
            const auto* selected = answer ? &pending_question_->yes : &pending_question_->no;
            pending_question_ = nullptr;
            stack_.push_back({selected, 0});
        }
    }
    for (std::uint32_t operations = 0; operations < 1024; ++operations) {
        if (stack_.empty()) { finished_ = true; return std::nullopt; }
        auto& frame = stack_.back();
        if (frame.cursor == frame.steps->size()) { stack_.pop_back(); continue; }
        const auto& action = (*frame.steps)[frame.cursor++];
        switch (action.kind) {
        case 0: waiting_ = true; return InteractionRequest{InteractionRequestKind::message, action.argument};
        case 1:
            if (action.argument == 5) {
                pending_question_ = &action; waiting_ = true;
                return InteractionRequest{InteractionRequestKind::question, 0};
            }
            if (stack_.size() >= 17) throw std::invalid_argument("interaction nesting exceeds its bound");
            stack_.push_back({condition(action) ? &action.yes : &action.no, 0});
            break;
        case 2: flags(action); break;
        case 3: waiting_ = true; return InteractionRequest{InteractionRequestKind::shop, action.argument};
        case 4: waiting_ = true; return InteractionRequest{InteractionRequestKind::callback, action.argument};
        default: throw std::invalid_argument("unsupported interaction action");
        }
    }
    throw std::runtime_error("interaction exceeded its bounded action budget");
}

bool Interaction::finished() const noexcept { return finished_; }
std::uint16_t Interaction::actor() const noexcept { return actor_; }

} // namespace dw4::game