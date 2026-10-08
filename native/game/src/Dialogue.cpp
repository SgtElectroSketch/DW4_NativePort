#include <dw4/game/Dialogue.hpp>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

namespace dw4::game {

std::vector<DialoguePage> layout_dialogue(const std::span<const std::uint8_t> symbols, const DialogueContext& context,
                                        const std::uint16_t columns, const std::uint16_t rows)
{
    if (columns == 0 || columns > 30 || rows == 0 || rows > 26 || symbols.size() > 4096) {
        throw std::invalid_argument("dialogue layout exceeds its explicit bounds");
    }
    std::vector<DialoguePage> pages(1);
    pages.back().lines.emplace_back();
    std::vector<std::uint16_t> word;
    bool space = false;
    const auto next_line = [&]() {
        if (pages.back().lines.size() == rows) { pages.emplace_back(); }
        pages.back().lines.emplace_back();
        space = false;
    };
    const auto next_page = [&]() {
        if (!pages.back().lines.back().empty() || pages.back().lines.size() > 1) {
            pages.emplace_back(); pages.back().lines.emplace_back();
        }
        space = false;
    };
    const auto flush_word = [&]() {
        if (word.empty()) return;
        if (pages.back().lines.back().size() + word.size() + (space ? 1U : 0U) > columns && !pages.back().lines.back().empty()) next_line();
        if (space && !pages.back().lines.back().empty()) pages.back().lines.back().push_back(0);
        for (const auto glyph : word) {
            if (pages.back().lines.back().size() == columns) next_line();
            pages.back().lines.back().push_back(glyph);
        }
        word.clear(); space = false;
    };
    for (std::size_t index = 0; index < symbols.size(); ++index) {
        const auto symbol = symbols[index];
        if (symbol == 0) { flush_word(); space = true; }
        else if (symbol == 0x40 || symbol == 0x45 || symbol == 0x46) { flush_word(); break; }
        else if (symbol == 0x41 || symbol == 0x43) { flush_word(); next_line(); }
        else if (symbol == 0x42 || symbol == 0x44 || symbol == 0x4F) {
            flush_word(); next_page();
            if (symbol == 0x4F) word.push_back(0x69);
        } else if (symbol == 0x47) {
            for (const auto digit : std::to_string(context.number)) word.push_back(static_cast<std::uint16_t>(digit - '0' + 1));
        } else if (symbol == 0x48 || symbol == 0x49) {
            if (++index == symbols.size()) throw std::invalid_argument("dialogue substitution has no operand");
            const auto operand = symbols[index];
            const std::vector<std::uint16_t>* value = nullptr;
            if (symbol == 0x48) {
                const auto found = context.lookup_names.find(operand);
                if (found != context.lookup_names.end()) value = &found->second;
            } else {
                const auto member = static_cast<std::uint8_t>(operand & 15U);
                const auto id = operand & 128U ? context.party[operand & 3U] : member;
                if (id < context.character_names.size()) value = &context.character_names[id];
            }
            if (value == nullptr || value->empty()) throw std::invalid_argument("dialogue substitution is unavailable");
            word.insert(word.end(), value->begin(), value->end());
        } else if (symbol == 0x4E) {
            flush_word();
        } else if (symbol >= 0x4A && symbol < 0x59) {
            throw std::invalid_argument("dialogue command requires an explicit native context");
        } else {
            word.push_back(symbol);
        }
    }
    flush_word();
    while (pages.size() > 1 && pages.back().lines.size() == 1 && pages.back().lines[0].empty()) pages.pop_back();
    if (pages.size() > 256) throw std::invalid_argument("dialogue produces too many pages");
    return pages;
}

Dialogue::Dialogue(std::vector<DialoguePage> pages, const std::uint8_t speed) : pages_(std::move(pages)), speed_(speed)
{
    if (pages_.empty() || pages_.size() > 256 || speed > 7) throw std::invalid_argument("dialogue requires bounded pages and speed");
    for (const auto& page : pages_) {
        if (page.lines.empty() || page.lines.size() > 26) throw std::invalid_argument("invalid dialogue page");
        for (const auto& line : page.lines) {
            if (line.size() > 30 || std::ranges::any_of(line, [](const auto glyph) { return glyph > 140; })) {
                throw std::invalid_argument("invalid dialogue glyph run");
            }
        }
    }
    if (speed == 0 || speed == 7) visible_ = page_size();
}

std::size_t Dialogue::page_size() const noexcept
{
    std::size_t count = 0;
    for (const auto& line : pages_[page_].lines) count += line.size();
    return count;
}

void Dialogue::tick(const InputFrame pressed)
{
    if (finished_) return;
    const bool advance = pressed.is_pressed(Button::a) || pressed.is_pressed(Button::b);
    if (visible_ < page_size()) {
        if (advance) { visible_ = page_size(); return; }
        if (delay_ == 0) { ++visible_; delay_ = speed_ == 0 ? 0 : static_cast<std::uint16_t>(speed_ - 1U); }
        else --delay_;
        return;
    }
    if (!advance) return;
    if (page_ + 1U == pages_.size()) { finished_ = true; return; }
    ++page_; delay_ = 0;
    visible_ = speed_ == 0 || speed_ == 7 ? page_size() : 0;
}

const DialoguePage& Dialogue::page() const { return pages_[page_]; }
std::size_t Dialogue::page_index() const noexcept { return page_; }
std::size_t Dialogue::visible_glyphs() const noexcept { return visible_; }
bool Dialogue::waiting() const noexcept { return !finished_ && visible_ == page_size(); }
bool Dialogue::finished() const noexcept { return finished_; }

} // namespace dw4::game