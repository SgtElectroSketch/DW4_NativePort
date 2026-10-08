#pragma once

#include <cstdint>
#include <random>

namespace dw4::game {

struct RandomState final {
    std::mt19937 engine{std::mt19937::default_seed};
    std::uint64_t draw_counter{};

    [[nodiscard]] bool operator==(const RandomState&) const noexcept = default;
};

class Random final {
public:
    explicit Random(RandomState initial) noexcept;

    [[nodiscard]] std::uint8_t next_byte() noexcept;
    [[nodiscard]] std::uint32_t next_below(std::uint32_t exclusive_upper_bound);
    [[nodiscard]] const RandomState& state() const noexcept;

private:
    RandomState state_;
};

} // namespace dw4::game