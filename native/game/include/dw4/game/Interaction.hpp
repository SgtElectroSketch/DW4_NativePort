#pragma once

#include <dw4/game/Field.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace dw4::game {

enum class InteractionRequestKind : std::uint8_t { message, question, shop, callback };

struct InteractionRequest final {
    InteractionRequestKind kind{};
    std::uint16_t value{};
};

class Interaction final {
public:
    Interaction(std::vector<FieldEffect> program, std::uint16_t actor, AdventureState& adventure);
    [[nodiscard]] std::optional<InteractionRequest> next(std::optional<bool> response = std::nullopt);
    [[nodiscard]] bool finished() const noexcept;
    [[nodiscard]] std::uint16_t actor() const noexcept;

private:
    struct Frame final { const std::vector<FieldEffect>* steps; std::size_t cursor{}; };
    [[nodiscard]] bool condition(const FieldEffect& action) const;
    [[nodiscard]] std::uint8_t flag_byte(std::uint16_t index) const;
    void flags(const FieldEffect& action);
    std::vector<FieldEffect> program_;
    std::vector<Frame> stack_;
    std::uint16_t actor_{};
    AdventureState& adventure_;
    const FieldEffect* pending_question_{};
    bool waiting_{};
    bool finished_{};
};

} // namespace dw4::game