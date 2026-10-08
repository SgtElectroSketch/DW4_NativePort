#pragma once

#include <dw4/game/MenuInput.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace dw4::game {

[[nodiscard]] std::optional<std::uint8_t> message_speed_delay(std::uint8_t speed);

enum class LogPage : std::uint8_t {
    commands, select_slot, select_destination, name, gender, speed, confirmation, message, ready, return_to_title,
};

enum class LogOperation : std::uint8_t { load, create, copy, erase, speed };

struct LogSlot final {
    bool occupied{};
    bool corrupt{};
    std::vector<std::uint16_t> name;
    std::uint8_t gender{};
    std::uint8_t speed{3};
    std::uint8_t chapter{1};
};

struct LogRequest final {
    LogOperation operation{};
    std::uint8_t source{};
    std::uint8_t destination{};
    std::vector<std::uint16_t> name;
    std::uint8_t gender{};
    std::uint8_t speed{3};
};

struct LogState final {
    LogPage page{LogPage::commands};
    LogOperation operation{LogOperation::create};
    std::uint16_t cursor{};
    std::uint8_t source{};
    std::uint8_t destination{};
    std::vector<std::uint16_t> name;
    std::uint8_t gender{};
    std::uint8_t speed{3};
    std::string message;
};

class AdventureLog final {
public:
    AdventureLog(std::array<LogSlot, 3> slots, std::vector<std::uint16_t> characters, bool can_continue = true);
    void tick(MenuActions actions);
    [[nodiscard]] const LogState& state() const noexcept;
    [[nodiscard]] const std::array<LogSlot, 3>& slots() const noexcept;
    [[nodiscard]] std::vector<LogOperation> commands() const;
    [[nodiscard]] const std::vector<std::uint16_t>& characters() const noexcept;
    [[nodiscard]] std::optional<LogRequest> take_request();
    void complete(std::array<LogSlot, 3> slots, std::string error = {});

private:
    struct Parent final { LogPage page; std::uint16_t cursor; };
    [[nodiscard]] std::vector<std::uint8_t> candidates() const;
    void message(std::string text);
    void open(LogPage page, std::uint16_t cursor = 0);
    void close();
    std::array<LogSlot, 3> slots_;
    std::vector<std::uint16_t> characters_;
    std::vector<Parent> parents_;
    LogState state_;
    std::optional<LogRequest> request_;
    bool waiting_{};
    bool can_continue_{};
};

} // namespace dw4::game