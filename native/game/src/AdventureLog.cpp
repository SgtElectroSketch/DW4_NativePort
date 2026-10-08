#include <dw4/game/AdventureLog.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace dw4::game {

std::optional<std::uint8_t> message_speed_delay(const std::uint8_t speed)
{
    constexpr std::array<std::uint8_t, 7> delays{15, 21, 27, 33, 40, 46, 54};
    if (speed == 7) return std::nullopt;
    if (speed >= delays.size()) throw std::out_of_range("message speed must be 0 through 7");
    return delays[speed];
}

AdventureLog::AdventureLog(std::array<LogSlot, 3> slots, std::vector<std::uint16_t> characters, const bool can_continue)
    : slots_(std::move(slots)), characters_(std::move(characters)), can_continue_(can_continue)
{
    if (characters_.empty() || characters_.size() > 128) throw std::invalid_argument("name keyboard requires bounded characters");
    for (const auto glyph : characters_) {
        if (glyph > 140) throw std::invalid_argument("name keyboard contains an invalid glyph");
    }
}

std::vector<LogOperation> AdventureLog::commands() const
{
    const auto occupied = std::ranges::count_if(slots_, [](const LogSlot& slot) { return slot.occupied && !slot.corrupt; });
    const auto empty = std::ranges::count_if(slots_, [](const LogSlot& slot) { return !slot.occupied && !slot.corrupt; });
    std::vector<LogOperation> commands;
    if (occupied > 0 && can_continue_) commands.push_back(LogOperation::load);
    if (occupied > 0) commands.push_back(LogOperation::speed);
    if (empty > 0) commands.push_back(LogOperation::create);
    if (occupied > 0 && empty > 0) commands.push_back(LogOperation::copy);
    if (occupied > 0 || std::ranges::any_of(slots_, [](const LogSlot& slot) { return slot.corrupt; })) commands.push_back(LogOperation::erase);
    return commands;
}

std::vector<std::uint8_t> AdventureLog::candidates() const
{
    std::vector<std::uint8_t> result;
    for (std::uint8_t slot = 0; slot < 3; ++slot) {
        const auto& value = slots_[slot];
        const bool empty = !value.occupied && !value.corrupt;
        if ((state_.page == LogPage::select_destination || state_.operation == LogOperation::create) ? empty :
            (state_.operation == LogOperation::erase ? value.occupied || value.corrupt : value.occupied && !value.corrupt)) {
            result.push_back(slot);
        }
    }
    return result;
}

void AdventureLog::message(std::string text)
{
    state_.message = std::move(text);
    state_.page = LogPage::message;
    state_.cursor = 0;
}

void AdventureLog::open(const LogPage page, const std::uint16_t cursor)
{
    parents_.push_back({state_.page, state_.cursor});
    state_.page = page;
    state_.cursor = cursor;
}

void AdventureLog::close()
{
    if (parents_.empty()) { state_.page = LogPage::commands; state_.cursor = 0; return; }
    const auto parent = parents_.back();
    parents_.pop_back();
    state_.page = parent.page;
    state_.cursor = parent.cursor;
}

void AdventureLog::tick(const MenuActions actions)
{
    if (waiting_) return;
    if (state_.page == LogPage::message || state_.page == LogPage::ready) {
        if (actions.confirm || actions.cancel) { parents_.clear(); state_.page = LogPage::commands; state_.cursor = 0; }
        return;
    }
    if (actions.cancel) {
        if (state_.page == LogPage::commands) { state_.page = LogPage::return_to_title; return; }
        if (state_.page == LogPage::name && !state_.name.empty()) { state_.name.pop_back(); return; }
        close();
        return;
    }
    if (state_.page == LogPage::name) {
        const auto count = static_cast<std::uint16_t>(characters_.size() + 2U);
        if (actions.left) state_.cursor = static_cast<std::uint16_t>((state_.cursor + count - 1U) % count);
        if (actions.right) state_.cursor = static_cast<std::uint16_t>((state_.cursor + 1U) % count);
        if (actions.up) state_.cursor = static_cast<std::uint16_t>((state_.cursor + count - 11U) % count);
        if (actions.down) state_.cursor = static_cast<std::uint16_t>((state_.cursor + 11U) % count);
        if (actions.confirm) {
            if (state_.cursor == characters_.size()) { if (!state_.name.empty()) state_.name.pop_back(); }
            else if (state_.cursor == characters_.size() + 1U) {
                if (std::ranges::any_of(state_.name, [](const std::uint16_t glyph) { return glyph != 0; })) {
                    open(LogPage::gender);
                }
            } else if (state_.name.size() < 8) state_.name.push_back(characters_[state_.cursor]);
        }
        return;
    }
    std::size_t count = 0;
    if (state_.page == LogPage::commands) count = commands().size();
    else if (state_.page == LogPage::select_slot || state_.page == LogPage::select_destination) count = candidates().size();
    else if (state_.page == LogPage::gender || state_.page == LogPage::confirmation) count = 2;
    else if (state_.page == LogPage::speed) count = 8;
    if (count == 0) { message("No adventure logs are available."); return; }
    if (actions.up || actions.left) state_.cursor = static_cast<std::uint16_t>((state_.cursor + count - 1U) % count);
    if (actions.down || actions.right) state_.cursor = static_cast<std::uint16_t>((state_.cursor + 1U) % count);
    if (!actions.confirm) return;
    switch (state_.page) {
    case LogPage::commands:
        state_.operation = commands()[state_.cursor]; open(LogPage::select_slot); break;
    case LogPage::select_slot:
        state_.source = candidates()[state_.cursor];
        if (state_.operation == LogOperation::create) { state_.destination = state_.source; state_.name.clear(); open(LogPage::name); }
        else if (state_.operation == LogOperation::copy) open(LogPage::select_destination);
        else if (state_.operation == LogOperation::speed) { state_.speed = slots_[state_.source].speed; open(LogPage::speed, state_.speed); }
        else open(LogPage::confirmation);
        break;
    case LogPage::select_destination:
        state_.destination = candidates()[state_.cursor]; open(LogPage::confirmation, 1); break;
    case LogPage::gender:
        state_.gender = static_cast<std::uint8_t>(state_.cursor); open(LogPage::speed, state_.speed); break;
    case LogPage::speed:
        state_.speed = static_cast<std::uint8_t>(state_.cursor); open(LogPage::confirmation); break;
    case LogPage::confirmation:
        if (state_.cursor == 1) { close(); break; }
        request_ = LogRequest{state_.operation, state_.source, state_.destination, state_.name, state_.gender, state_.speed};
        waiting_ = true;
        break;
    default: break;
    }
}

const LogState& AdventureLog::state() const noexcept { return state_; }
const std::array<LogSlot, 3>& AdventureLog::slots() const noexcept { return slots_; }
const std::vector<std::uint16_t>& AdventureLog::characters() const noexcept { return characters_; }
std::optional<LogRequest> AdventureLog::take_request() { auto result = std::move(request_); request_.reset(); return result; }

void AdventureLog::complete(std::array<LogSlot, 3> slots, std::string error)
{
    waiting_ = false;
    slots_ = std::move(slots);
    if (!error.empty()) { message(std::move(error)); return; }
    message(state_.operation == LogOperation::erase ? "Adventure log erased." :
            state_.operation == LogOperation::copy ? "Adventure log copied." :
            state_.operation == LogOperation::speed ? "Message speed changed." : "Adventure log saved.");
    if (state_.operation == LogOperation::load) state_.page = LogPage::ready;
}

} // namespace dw4::game