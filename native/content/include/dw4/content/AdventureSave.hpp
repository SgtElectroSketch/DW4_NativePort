#pragma once

#include <cstdint>
#include <array>
#include <filesystem>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace dw4::content {

struct SavedItem final {
    std::uint16_t id{};
    std::uint8_t equipped{};
    bool operator==(const SavedItem&) const = default;
};

struct SavedCharacter final {
    std::uint8_t id{};
    std::uint8_t level{1};
    std::uint16_t hp{}, maximum_hp{}, mp{}, maximum_mp{};
    std::uint16_t strength{}, agility{}, vitality{}, intelligence{}, luck{};
    std::uint32_t experience{};
    std::uint16_t conditions{};
    std::vector<SavedItem> inventory;
    std::vector<std::uint16_t> spells;
    bool operator==(const SavedCharacter&) const = default;
};

struct SavedActor final {
    std::uint16_t id{}, sprite{};
    std::int16_t x{}, y{}, target_x{}, target_y{};
    std::uint8_t facing{}, step{};
    std::uint16_t instruction{}, delay{};
    bool active{true};
    bool operator==(const SavedActor&) const = default;
};

struct SavedWorld final {
    std::uint8_t map{}, submap{}, facing{};
    std::int16_t x{}, y{};
    std::array<SavedCharacter, 9> characters;
    std::array<std::uint8_t, 4> party{6, 255, 255, 255};
    std::uint32_t gold{}, bank_gold{}, casino_coins{};
    std::vector<SavedItem> bag;
    std::vector<std::uint16_t> story_flags;
    std::vector<SavedActor> actors;
    std::uint16_t event_step{}, event_delay{};
    std::uint8_t phase{};
    bool operator==(const SavedWorld&) const = default;
};

struct AdventureSave final {
    std::vector<std::uint16_t> name;
    std::uint8_t gender{};
    std::uint8_t message_speed{3};
    std::uint8_t chapter{1};
    std::uint64_t frame{};
    std::mt19937 random_engine{std::mt19937::default_seed};
    std::uint64_t random_draws{};
    std::optional<SavedWorld> world;

    [[nodiscard]] bool operator==(const AdventureSave&) const noexcept = default;
};

class AdventureSaveFile final {
public:
    [[nodiscard]] static AdventureSave load(const std::filesystem::path& path);
    static void save(const std::filesystem::path& path, const AdventureSave& state);
};

} // namespace dw4::content