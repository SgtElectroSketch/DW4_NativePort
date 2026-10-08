#pragma once

#include <dw4/game/World.hpp>
#include <dw4/game/Game.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace dw4::game {

struct FieldEffect final {
    std::uint8_t kind{};
    std::uint16_t argument{}, value{}, extra{};
    bool invert{};
    std::vector<FieldEffect> yes, no;
};

struct ActorDefinition final {
    std::uint16_t id{}, sprite{};
    std::int16_t x{}, y{};
    Direction facing{Direction::down};
    std::uint8_t motion{};
    std::vector<FieldEffect> interaction;
    std::vector<std::array<std::int16_t, 2>> patrol;
    bool follow_player{};
    bool remove_at_end{};
};

struct ActorState final {
    std::uint16_t id{}, sprite{};
    std::int16_t x{}, y{}, target_x{}, target_y{};
    Direction facing{Direction::down};
    std::uint8_t step{};
    std::uint16_t instruction{}, delay{};
    bool active{true};
    bool operator==(const ActorState&) const = default;
};

struct MapConnection final {
    std::int16_t x{}, y{};
    MapPosition destination;
};

struct TerrainMap final {
    std::uint8_t id{}, submap{};
    std::uint16_t width{}, height{};
    std::vector<std::uint8_t> cells;
    std::array<std::uint8_t, 48> behaviors{};
    std::vector<ActorDefinition> actors;
    std::vector<MapConnection> connections;
    std::optional<MapPosition> boundary_destination;
};

struct FieldState final {
    AdventureState adventure;
    std::vector<ActorState> actors;
    std::int16_t player_offset_x{}, player_offset_y{};
    std::uint8_t player_step{};
    std::uint16_t event_step{}, event_delay{};
    std::uint8_t phase{};
    bool operator==(const FieldState&) const = default;
};

class Field final {
public:
    Field(std::vector<TerrainMap> maps, FieldState initial);
    void tick(InputFrame held);
    [[nodiscard]] const FieldState& state() const noexcept;
    [[nodiscard]] FieldState& state_for_session() noexcept;
    [[nodiscard]] const TerrainMap& map() const;
    [[nodiscard]] bool passable(std::int16_t x, std::int16_t y, bool actors = true) const;
    [[nodiscard]] std::optional<std::uint16_t> actor_in_front() const;
    [[nodiscard]] std::uint8_t roof_group() const;
    void set_flag(std::uint16_t id, bool set);
    [[nodiscard]] bool flag(std::uint16_t id) const;
    void refresh_actors();
    void restart_actor(std::uint16_t id);
    void tick_presentation_actors();

private:
    void enter(MapPosition position);
    void tick_actors();
    std::vector<TerrainMap> maps_;
    FieldState state_;
};

} // namespace dw4::game