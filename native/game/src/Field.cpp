#include <dw4/game/Field.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace dw4::game {
namespace {

constexpr std::array<std::int16_t, 4> direction_x{0, 1, 0, -1};
constexpr std::array<std::int16_t, 4> direction_y{-1, 0, 1, 0};

std::optional<Direction> movement_direction(const InputFrame input)
{
    const bool up = input.is_pressed(Button::up), down = input.is_pressed(Button::down);
    const bool left = input.is_pressed(Button::left), right = input.is_pressed(Button::right);
    if (up != down) return up ? Direction::up : Direction::down;
    if (left != right) return left ? Direction::left : Direction::right;
    return std::nullopt;
}

} // namespace

Field::Field(std::vector<TerrainMap> maps, FieldState initial) : maps_(std::move(maps)), state_(std::move(initial))
{
    if (maps_.empty() || maps_.size() > 512) throw std::invalid_argument("field requires bounded map rules");
    for (const auto& map : maps_) {
        if (map.width == 0 || map.height == 0 || map.cells.size() != static_cast<std::size_t>(map.width) * map.height || map.actors.size() > 128) {
            throw std::invalid_argument("invalid field map rules");
        }
    }
    const auto& current = map();
    if (state_.adventure.position.x < 0 || state_.adventure.position.y < 0 || state_.adventure.position.x >= current.width || state_.adventure.position.y >= current.height) {
        throw std::invalid_argument("field resume position is outside its map");
    }
    if (state_.actors.empty()) enter(state_.adventure.position);
}

const TerrainMap& Field::map() const
{
    const auto& position = state_.adventure.position;
    const auto found = std::ranges::find_if(maps_, [&](const auto& map) { return map.id == position.map && map.submap == position.submap; });
    if (found == maps_.end()) throw std::out_of_range("field map rules are unavailable");
    return *found;
}

void Field::enter(const MapPosition position)
{
    state_.adventure.position = position;
    const auto& current = map();
    state_.actors.clear();
    for (const auto& actor : current.actors) {
        state_.actors.push_back({actor.id, actor.sprite, actor.x, actor.y, actor.x, actor.y, actor.facing});
    }
    state_.player_offset_x = state_.player_offset_y = 0;
    state_.player_step = 0;
}

bool Field::passable(const std::int16_t x, const std::int16_t y, const bool actors) const
{
    const auto& current = map();
    if (x < 0 || y < 0 || x >= current.width || y >= current.height) return false;
    const auto cell = current.cells[static_cast<std::size_t>(y) * current.width + x];
    const auto slot = current.id == 255 ? cell : cell & 31U;
    if (slot >= current.behaviors.size() || current.behaviors[slot] >= 128) return false;
    if (actors && std::ranges::any_of(state_.actors, [&](const auto& actor) { return actor.active && actor.x == x && actor.y == y; })) return false;
    return true;
}

void Field::tick(const InputFrame held)
{
    if (state_.phase != 0) return;
    tick_actors();
    if (state_.player_step > 0) {
        --state_.player_step;
        const auto direction = static_cast<std::size_t>(state_.adventure.facing);
        state_.player_offset_x = static_cast<std::int16_t>(-direction_x[direction] * state_.player_step * 2);
        state_.player_offset_y = static_cast<std::int16_t>(-direction_y[direction] * state_.player_step * 2);
        if (state_.player_step == 0) {
            const auto& position = state_.adventure.position;
            const auto& connections = map().connections;
            const auto found = std::ranges::find_if(connections, [&](const auto& connection) { return connection.x == position.x && connection.y == position.y; });
            if (found != connections.end()) enter(found->destination);
        }
        return;
    }
    const auto direction = movement_direction(held);
    if (!direction) return;
    state_.adventure.facing = *direction;
    const auto index = static_cast<std::size_t>(*direction);
    const auto& position = state_.adventure.position;
    const auto x = static_cast<std::int16_t>(position.x + direction_x[index]);
    const auto y = static_cast<std::int16_t>(position.y + direction_y[index]);
    const auto& current = map();
    if ((x < 0 || y < 0 || x >= current.width || y >= current.height) && current.boundary_destination) {
        const auto destination = *current.boundary_destination;
        enter(destination);
        return;
    }
    if (!passable(x, y)) return;
    state_.adventure.position.x = x;
    state_.adventure.position.y = y;
    state_.player_step = 8;
    state_.player_offset_x = static_cast<std::int16_t>(-direction_x[index] * 16);
    state_.player_offset_y = static_cast<std::int16_t>(-direction_y[index] * 16);
}

void Field::tick_actors()
{
    for (auto& actor : state_.actors) {
        if (!actor.active) continue;
        if (actor.step != 0) { --actor.step; continue; }
        if (actor.delay != 0) { --actor.delay; continue; }
        const auto found = std::ranges::find_if(map().actors, [&](const auto& value) { return value.id == actor.id; });
        if (found == map().actors.end() || (found->patrol.empty() && !found->follow_player)) continue;
        std::array<std::int16_t, 2> target;
        if (found->follow_player) {
            target = {state_.adventure.position.x, state_.adventure.position.y};
            if (std::abs(actor.x - target[0]) + std::abs(actor.y - target[1]) <= 1) continue;
        } else {
            if (found->remove_at_end && actor.instruction >= found->patrol.size()) { actor.active = false; continue; }
            actor.instruction %= static_cast<std::uint16_t>(found->patrol.size());
            target = found->patrol[actor.instruction];
        }
        if (actor.x == target[0] && actor.y == target[1]) {
            if (!found->patrol.empty()) actor.instruction = found->remove_at_end ? static_cast<std::uint16_t>(actor.instruction + 1U) :
                static_cast<std::uint16_t>((actor.instruction + 1U) % found->patrol.size());
            actor.delay = 8;
            continue;
        }
        const auto direction = actor.y != target[1] ? (actor.y > target[1] ? Direction::up : Direction::down) :
                                                     (actor.x > target[0] ? Direction::left : Direction::right);
        const auto index = static_cast<std::size_t>(direction);
        const auto x = static_cast<std::int16_t>(actor.x + direction_x[index]);
        const auto y = static_cast<std::int16_t>(actor.y + direction_y[index]);
        actor.facing = direction;
        if ((x == state_.adventure.position.x && y == state_.adventure.position.y) || !passable(x, y)) { actor.delay = 8; continue; }
        actor.target_x = actor.x; actor.target_y = actor.y;
        actor.x = x; actor.y = y; actor.step = 8;
    }
}

std::optional<std::uint16_t> Field::actor_in_front() const
{
    const auto index = static_cast<std::size_t>(state_.adventure.facing);
    const auto& current = map();
    int distance = 1;
    for (int step = 1; step <= distance; ++step) {
        const auto x = state_.adventure.position.x + direction_x[index] * step;
        const auto y = state_.adventure.position.y + direction_y[index] * step;
        const auto found = std::ranges::find_if(state_.actors, [&](const auto& actor) { return actor.active && actor.x == x && actor.y == y; });
        if (found != state_.actors.end()) return found->id;
        if (step == 1 && x >= 0 && y >= 0 && x < current.width && y < current.height) {
            const auto cell = current.cells[static_cast<std::size_t>(y) * current.width + x] & 31U;
            const auto behavior = current.behaviors[cell] & 127U;
            if (behavior == 0x27) distance = 2;
            else if (behavior == 0x17) distance = 3;
        }
    }
    return std::nullopt;
}

std::uint8_t Field::roof_group() const
{
    const auto& current = map();
    if (current.id == 255) return 0;
    const auto& position = state_.adventure.position;
    return current.cells[static_cast<std::size_t>(position.y) * current.width + position.x] >> 5;
}

void Field::set_flag(const std::uint16_t id, const bool set)
{
    auto& flags = state_.adventure.story_flags;
    const auto found = std::ranges::find(flags, id);
    if (set && found == flags.end()) flags.push_back(id);
    if (!set && found != flags.end()) flags.erase(found);
}

bool Field::flag(const std::uint16_t id) const { return std::ranges::find(state_.adventure.story_flags, id) != state_.adventure.story_flags.end(); }
void Field::refresh_actors()
{
    const auto position = state_.adventure.position;
    enter(position);
}

void Field::restart_actor(const std::uint16_t id)
{
    const auto actor = std::ranges::find_if(state_.actors, [&](const auto& value) { return value.id == id; });
    if (actor == state_.actors.end()) throw std::out_of_range("scripted actor is unavailable");
    actor->instruction = 0; actor->delay = 0; actor->step = 0;
    actor->target_x = actor->x; actor->target_y = actor->y;
    actor->active = true;
}

const FieldState& Field::state() const noexcept { return state_; }
void Field::tick_presentation_actors() { tick_actors(); }
FieldState& Field::state_for_session() noexcept { return state_; }

} // namespace dw4::game