#include <dw4/game/Random.hpp>

#include <stdexcept>

namespace dw4::game {

Random::Random(const RandomState initial) noexcept
    : state_(initial)
{
}

std::uint8_t Random::next_byte() noexcept
{
    std::uniform_int_distribution<std::uint32_t> distribution(0, 255);
    const auto value = distribution(state_.engine);
    ++state_.draw_counter;
    return static_cast<std::uint8_t>(value);
}

std::uint32_t Random::next_below(const std::uint32_t exclusive_upper_bound)
{
    if (exclusive_upper_bound == 0) {
        throw std::invalid_argument("random upper bound must be positive");
    }
    std::uniform_int_distribution<std::uint32_t> distribution(0, exclusive_upper_bound - 1U);
    const auto value = distribution(state_.engine);
    ++state_.draw_counter;
    return value;
}

const RandomState& Random::state() const noexcept
{
    return state_;
}

} // namespace dw4::game