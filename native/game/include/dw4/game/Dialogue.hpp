#pragma once

#include <dw4/game/Game.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <span>
#include <vector>

namespace dw4::game {

struct DialogueContext final {
    std::array<std::vector<std::uint16_t>, 9> character_names;
    std::array<std::uint8_t, 4> party{6, 255, 255, 255};
    std::map<std::uint16_t, std::vector<std::uint16_t>> lookup_names;
    std::uint32_t number{};
};

struct DialoguePage final {
    std::vector<std::vector<std::uint16_t>> lines;
    bool operator==(const DialoguePage&) const = default;
};

[[nodiscard]] std::vector<DialoguePage> layout_dialogue(std::span<const std::uint8_t> symbols,
    const DialogueContext& context = {}, std::uint16_t columns = 26, std::uint16_t rows = 8);

class Dialogue final {
public:
    explicit Dialogue(std::vector<DialoguePage> pages, std::uint8_t speed = 3);
    void tick(InputFrame pressed);
    [[nodiscard]] const DialoguePage& page() const;
    [[nodiscard]] std::size_t page_index() const noexcept;
    [[nodiscard]] std::size_t visible_glyphs() const noexcept;
    [[nodiscard]] bool waiting() const noexcept;
    [[nodiscard]] bool finished() const noexcept;

private:
    [[nodiscard]] std::size_t page_size() const noexcept;
    std::vector<DialoguePage> pages_;
    std::size_t page_{};
    std::size_t visible_{};
    std::uint16_t delay_{};
    std::uint8_t speed_{};
    bool finished_{};
};

} // namespace dw4::game