#include <dw4/game/Encounters.hpp>

#include <algorithm>
#include <stdexcept>

namespace dw4::game {

std::optional<EncounterFormation> roll_world_encounter(const EncounterRules& rules, const std::uint8_t x, const std::uint8_t y,
    const std::uint8_t terrain, const std::uint8_t steps_since_battle, const bool night, Random& random)
{
    if (terrain >= 8) return std::nullopt;
    const auto zone = rules.grid[(y & 240U) | (x >> 4)] & 63U;
    const auto& record = rules.groups[zone];
    const auto multiplier = steps_since_battle < 3 ? rules.first_steps[steps_since_battle] : 16;
    const auto threshold = static_cast<std::uint32_t>(record.rate) * multiplier * rules.terrain_rates[terrain] / 256U;
    if (random.next_byte() >= threshold) return std::nullopt;
    const std::uint16_t mask = night ? 0x3FBE : 0x1BEF;
    std::uint32_t total = 0;
    for (std::size_t slot = 0; slot < 14; ++slot) if ((mask & (1U << slot)) != 0 && record.entries[slot] != 255) total += record.weights[slot];
    if (total == 0) return std::nullopt;
    auto choice = random.next_below(total);
    std::size_t selected = 0;
    for (; selected < 14; ++selected) {
        if ((mask & (1U << selected)) == 0 || record.entries[selected] == 255) continue;
        if (choice < record.weights[selected]) break;
        choice -= record.weights[selected];
    }
    if (selected == 14) throw std::logic_error("encounter weight selection exceeded its table");
    EncounterFormation result{{}, record.control};
    const auto add = [&](const std::uint16_t id, const std::uint32_t count) {
        if (id == 255 || count == 0) return;
        const auto found = std::ranges::find_if(result.groups, [&](const auto& group) { return group.monster == id; });
        if (found == result.groups.end()) result.groups.push_back({id, static_cast<std::uint8_t>(std::min<std::uint32_t>(8, count))});
        else found->count = static_cast<std::uint8_t>(std::min<std::uint32_t>(8, found->count + count));
    };
    if (selected < 5) {
        add(record.entries[selected], 1);
        std::uint32_t other = random.next_below(total);
        std::uint16_t next = record.entries[selected];
        for (std::size_t slot = 0; slot < 14; ++slot) {
            if ((mask & (1U << slot)) == 0 || record.entries[slot] == 255) continue;
            if (other < record.weights[slot]) { next = slot < 12 ? record.entries[slot] : record.entries[selected]; break; }
            other -= record.weights[slot];
        }
        add(next, 1);
        for (std::size_t slot = 0; slot < 2; ++slot) if (random.next_byte() < record.extra[slot]) add(next, 1);
    } else if (selected < 11 && selected != 5) {
        const auto packed = static_cast<std::uint16_t>(record.extra[2] | (static_cast<std::uint16_t>(record.extra[3]) << 8));
        const auto code = (packed >> ((selected - 6U) * 3U)) & 7U;
        const auto range = rules.single_span[code];
        add(record.entries[selected], rules.single_base[code] + (range == 0 ? 0 : random.next_below(range)));
    } else if (selected == 11) add(record.entries[selected], 1);
    else if (selected >= 12) {
        const auto& mixed = rules.mixed[record.entries[selected]];
        for (std::size_t slot = 0; slot < 4; ++slot) {
            const auto code = (mixed[slot / 2U] >> ((slot & 1U) * 3U)) & 7U;
            const auto range = rules.mixed_span[code];
            add(mixed[slot + 2U], rules.mixed_base[code] + (range == 0 ? 0 : random.next_below(range)));
        }
    } else add(record.entries[0], 1);
    if (result.groups.empty()) return std::nullopt;
    return result;
}

} // namespace dw4::game