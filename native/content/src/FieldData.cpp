#include <dw4/content/FieldData.hpp>

#include "JsonValidation.hpp"

#include <algorithm>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>

namespace dw4::content {
namespace {

using detail::Json;
using detail::unsigned_field;
using detail::unsigned_value;

const Json& list(const Json& object, const char* field, const std::size_t maximum, const std::filesystem::path& path)
{
    if (!object.is_object() || !object.contains(field) || !object.at(field).is_array() || object.at(field).size() > maximum) {
        throw std::runtime_error("invalid field collection " + std::string(field) + ": " + path.string());
    }
    return object.at(field);
}

std::array<FieldPalette, 4> palettes(const Json& source, const std::filesystem::path& path)
{
    if (!source.is_array() || source.size() != 4) throw std::runtime_error("field requires four palettes");
    std::array<FieldPalette, 4> result;
    for (std::size_t palette = 0; palette < 4; ++palette) {
        if (!source[palette].is_array() || source[palette].size() != 4) throw std::runtime_error("invalid field palette");
        for (std::size_t color = 0; color < 4; ++color) {
            if (!source[palette][color].is_array() || source[palette][color].size() != 4) throw std::runtime_error("invalid field RGBA color");
            for (std::size_t channel = 0; channel < 4; ++channel) {
                result[palette][color][channel] = static_cast<std::uint8_t>(unsigned_value(source[palette][color][channel], 255, "color", path));
            }
        }
    }
    return result;
}

std::array<std::uint16_t, 4> quadrants(const Json& source, const std::uint16_t count, const std::filesystem::path& path)
{
    const auto& values = list(source, "quadrants", 4, path);
    if (values.size() != 4) throw std::runtime_error("field metatile requires four quadrants");
    std::array<std::uint16_t, 4> result;
    for (std::size_t index = 0; index < 4; ++index) result[index] = static_cast<std::uint16_t>(unsigned_value(values[index], count - 1U, "quadrant", path));
    return result;
}

std::vector<FieldActor::Effect> effects(const Json& source, const std::filesystem::path& path, const std::uint32_t depth = 0)
{
    if (depth > 16 || !source.is_array() || source.size() > 256) throw std::runtime_error("invalid field interaction tree");
    std::vector<FieldActor::Effect> result;
    for (const auto& step : source) {
        if (!step.is_object() || !step.contains("kind") || !step.at("kind").is_string()) throw std::runtime_error("invalid interaction action");
        const auto kind = step.at("kind").get<std::string>();
        FieldActor::Effect action;
        if (kind == "say") {
            action.kind = 0; action.argument = static_cast<std::uint16_t>(unsigned_field(step, "message", 2815, "interaction.message", path));
        } else if (kind == "branch") {
            action.kind = 1; action.argument = static_cast<std::uint16_t>(unsigned_field(step, "condition", 63, "interaction.condition", path));
            if (!step.contains("invert") || !step.at("invert").is_boolean() || !step.contains("yes") || !step.contains("no")) throw std::runtime_error("invalid interaction branch");
            action.invert = step.at("invert").get<bool>();
            const auto& arguments = list(step, "arguments", 2, path);
            if (!arguments.empty()) action.value = static_cast<std::uint16_t>(unsigned_value(arguments[0], 255, "condition.value", path));
            if (arguments.size() > 1) action.extra = static_cast<std::uint16_t>(unsigned_value(arguments[1], 255, "condition.extra", path));
            action.yes = effects(step.at("yes"), path, depth + 1);
            action.no = effects(step.at("no"), path, depth + 1);
        } else if (kind == "flags") {
            action.kind = 2; action.argument = static_cast<std::uint16_t>(unsigned_field(step, "operation", 2, "flags.operation", path));
            action.value = static_cast<std::uint16_t>(unsigned_field(step, "index", 255, "flags.index", path));
            action.extra = static_cast<std::uint16_t>(unsigned_field(step, "mask", 255, "flags.mask", path));
        } else if (kind == "shop") {
            action.kind = 3; action.argument = static_cast<std::uint16_t>(unsigned_field(step, "mode", 6, "shop.mode", path));
        } else if (kind == "callback") {
            action.kind = 4;
            if (!step.contains("action") || !step.at("action").is_string()) throw std::runtime_error("callback requires reviewed action semantics");
            const auto callback = step.at("action").get<std::string>();
            if (callback == "sell_counter") action.argument = 0;
            else if (callback == "purchase_animation") action.argument = 1;
            else if (callback == "refresh_actors") action.argument = 2;
            else throw std::runtime_error("unsupported field interaction callback");
        } else throw std::runtime_error("unsupported field interaction action");
        result.push_back(std::move(action));
    }
    return result;
}

template<std::size_t Count>
std::array<std::uint8_t, Count> bytes(const Json& source, const char* key, const std::filesystem::path& path)
{
    const auto& values = list(source, key, Count, path);
    if (values.size() != Count) throw std::runtime_error("field byte table has the wrong length");
    std::array<std::uint8_t, Count> result;
    for (std::size_t index = 0; index < Count; ++index) result[index] = static_cast<std::uint8_t>(unsigned_value(values[index], 255, key, path));
    return result;
}

} // namespace

FieldData FieldData::load(const std::filesystem::path& directory)
{
    const auto path = directory / "index.json";
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error || size > 32U * 1024U * 1024U) throw std::runtime_error("field data is missing or oversized: " + path.string());
    std::ifstream input(path);
    Json document;
    try { input >> document; } catch (const Json::parse_error&) { throw std::runtime_error("field JSON is invalid: " + path.string()); }
    input >> std::ws;
    if (!input.eof() || unsigned_field(document, "schema_version", 1, "schema_version", path) != 1 ||
        !document.contains("image") || !document.at("image").is_string()) throw std::runtime_error("field data schema is unsupported");
    FieldData data;
    data.image_ = document.at("image").get<std::string>();
    if (data.image_.empty() || data.image_.has_root_path() || data.image_.extension() != ".png" ||
        std::ranges::any_of(data.image_, [](const auto& component) { return component == ".."; })) throw std::runtime_error("unsafe field atlas path");
    data.tile_count_ = static_cast<std::uint16_t>(unsigned_field(document, "tile_count", 8192, "tile_count", path));
    if (data.tile_count_ == 0) throw std::runtime_error("field atlas cannot be empty");
    if (!document.contains("sprite_palettes")) throw std::runtime_error("field sprite palettes are missing");
    data.sprite_palettes_ = palettes(document.at("sprite_palettes"), path);
    if (document.contains("items")) {
        std::set<std::uint16_t> ids;
        for (const auto& source : list(document, "items", 127, path)) {
            FieldItem item;
            item.id = static_cast<std::uint16_t>(unsigned_field(source, "id", 126, "item.id", path));
            if (!ids.insert(item.id).second || !source.contains("name") || !source.at("name").is_string() ||
                !source.contains("cursed") || !source.at("cursed").is_boolean()) throw std::runtime_error("invalid item identity or metadata");
            item.name = source.at("name").get<std::string>();
            if ((item.name.empty() && item.id != 0x51) || item.name.size() > 64) throw std::runtime_error("invalid item display name");
            item.price = static_cast<std::uint32_t>(unsigned_field(source, "price", 9999999, "item.price", path));
            item.power = static_cast<std::uint16_t>(unsigned_field(source, "power", 255, "item.power", path));
            item.slot = static_cast<std::uint8_t>(unsigned_field(source, "slot", 5, "item.slot", path));
            item.equip_mask = static_cast<std::uint8_t>(unsigned_field(source, "equip_mask", 255, "item.equip_mask", path));
            item.cursed = source.at("cursed").get<bool>();
            data.items_.push_back(std::move(item));
        }
    }
    if (document.contains("progression")) {
        std::set<std::uint8_t> ids;
        for (const auto& source : list(document, "progression", 9, path)) {
            FieldGrowth growth;
            growth.id = static_cast<std::uint8_t>(unsigned_field(source, "id", 8, "growth.id", path));
            if (!ids.insert(growth.id).second) throw std::runtime_error("duplicate character progression curve");
            const auto& levels = list(source, "levels", 99, path);
            if (levels.size() != 99) throw std::runtime_error("progression requires levels one through ninety-nine");
            for (std::size_t index = 0; index < levels.size(); ++index) {
                if (unsigned_field(levels[index], "level", 99, "growth.level", path) != index + 1U) throw std::runtime_error("progression levels must be ordered");
                auto& level = growth.levels[index];
                level.experience = static_cast<std::uint32_t>(unsigned_field(levels[index], "experience", 16777215, "growth.experience", path));
                if ((index == 0 && level.experience != 0) || (index > 0 && level.experience <= growth.levels[index - 1].experience)) throw std::runtime_error("progression experience thresholds must increase");
                const auto& targets = list(levels[index], "targets", 7, path);
                const auto& gains = list(levels[index], "gains", 7, path);
                if (targets.size() != 7 || gains.size() != 7) throw std::runtime_error("progression requires all seven attributes");
                for (std::size_t stat = 0; stat < 7; ++stat) {
                    level.targets[stat] = static_cast<std::uint16_t>(unsigned_value(targets[stat], 65535, "growth.target", path));
                    level.gains[stat] = static_cast<std::uint16_t>(unsigned_value(gains[stat], 65535, "growth.gain", path));
                }
            }
            data.progression_.push_back(std::move(growth));
        }
    }
    std::set<std::uint16_t> sprite_ids;
    if (document.contains("inns")) for (const auto& source : list(document, "inns", 128, path)) {
        data.inns_.push_back({static_cast<std::uint8_t>(unsigned_field(source, "map", 255, "inn.map", path)),
            static_cast<std::uint8_t>(unsigned_field(source, "submap", 255, "inn.submap", path)),
            static_cast<std::uint8_t>(unsigned_field(source, "price", 255, "inn.price", path))});
    }
    if (document.contains("treasures")) for (const auto& source : list(document, "treasures", 1024, path)) {
        FieldTreasure treasure;
        treasure.map = static_cast<std::uint8_t>(unsigned_field(source, "map", 72, "treasure.map", path));
        treasure.submap = static_cast<std::uint8_t>(unsigned_field(source, "submap", 31, "treasure.submap", path));
        treasure.x = static_cast<std::int16_t>(unsigned_field(source, "x", 1023, "treasure.x", path));
        treasure.y = static_cast<std::int16_t>(unsigned_field(source, "y", 1023, "treasure.y", path));
        treasure.flag = static_cast<std::uint16_t>(unsigned_field(source, "flag", 65535, "treasure.flag", path));
        treasure.gold = static_cast<std::uint32_t>(unsigned_field(source, "gold", 9999999, "treasure.gold", path));
        if (source.contains("item") && !source.at("item").is_null()) treasure.item = static_cast<std::uint16_t>(unsigned_value(source.at("item"), 126, "treasure.item", path));
        data.treasures_.push_back(treasure);
    }
    if (document.contains("encounters")) {
        const auto& source = document.at("encounters");
        auto& target = data.encounters_;
        target.grid = bytes<256>(source, "grid", path);
        target.terrain_rates = bytes<8>(source, "terrain_rates", path);
        target.first_steps = bytes<3>(source, "first_steps", path);
        target.mixed_span = bytes<8>(source, "mixed_span", path); target.mixed_base = bytes<8>(source, "mixed_base", path);
        target.single_span = bytes<8>(source, "single_span", path); target.single_base = bytes<8>(source, "single_base", path);
        const auto& groups = list(source, "groups", 64, path);
        if (groups.size() != 64) throw std::runtime_error("world encounters require 64 groups");
        for (std::size_t index = 0; index < 64; ++index) {
            if (unsigned_field(groups[index], "id", 63, "encounter.id", path) != index) throw std::runtime_error("encounter groups must be ordered");
            target.groups[index] = {static_cast<std::uint8_t>(unsigned_field(groups[index], "rate", 255, "encounter.rate", path)),
                static_cast<std::uint8_t>(unsigned_field(groups[index], "control", 255, "encounter.control", path)),
                bytes<14>(groups[index], "entries", path), bytes<14>(groups[index], "weights", path), bytes<4>(groups[index], "extra", path)};
        }
        const auto& mixed = list(source, "mixed", 256, path);
        if (mixed.size() != 256) throw std::runtime_error("mixed encounter records are incomplete");
        for (std::size_t index = 0; index < 256; ++index) {
            if (!mixed[index].is_array() || mixed[index].size() != 6) throw std::runtime_error("invalid mixed encounter record");
            for (std::size_t field = 0; field < 6; ++field) target.mixed[index][field] = static_cast<std::uint8_t>(unsigned_value(mixed[index][field], 255, "mixed", path));
        }
    }
    if (document.contains("monsters")) {
        std::set<std::uint16_t> ids;
        for (const auto& source : list(document, "monsters", 195, path)) {
            FieldMonster monster;
            monster.id = static_cast<std::uint16_t>(unsigned_field(source, "id", 194, "monster.id", path));
            if (!ids.insert(monster.id).second || !source.contains("name") || !source.at("name").is_string()) throw std::runtime_error("invalid monster identity");
            monster.name = source.at("name").get<std::string>();
            if (monster.name.empty() || monster.name.size() > 64) throw std::runtime_error("invalid monster name");
            monster.hp = static_cast<std::uint16_t>(unsigned_field(source, "hp", 1023, "monster.hp", path));
            monster.mp = static_cast<std::uint16_t>(unsigned_field(source, "mp", 255, "monster.mp", path));
            monster.attack = static_cast<std::uint16_t>(unsigned_field(source, "attack", 1023, "monster.attack", path));
            monster.defense = static_cast<std::uint16_t>(unsigned_field(source, "defense", 1023, "monster.defense", path));
            monster.agility = static_cast<std::uint16_t>(unsigned_field(source, "agility", 255, "monster.agility", path));
            monster.experience = static_cast<std::uint32_t>(unsigned_field(source, "experience", 65535, "monster.experience", path));
            monster.gold = static_cast<std::uint32_t>(unsigned_field(source, "gold", 1023, "monster.gold", path));
            if (source.contains("drop") && !source.at("drop").is_null()) {
                const auto& drop = source.at("drop");
                monster.drop = static_cast<std::uint16_t>(unsigned_field(drop, "item_id", 126, "drop.item", path));
                if (!drop.contains("chance")) throw std::runtime_error("monster drop chance is missing");
                monster.drop_numerator = static_cast<std::uint32_t>(unsigned_field(drop.at("chance"), "numerator", 65536, "drop.numerator", path));
                monster.drop_denominator = static_cast<std::uint32_t>(unsigned_field(drop.at("chance"), "denominator", 65536, "drop.denominator", path));
                if (monster.drop_denominator == 0 || monster.drop_numerator > monster.drop_denominator) throw std::runtime_error("invalid monster drop probability");
            }
            for (const auto& action : list(source, "actions", 6, path)) monster.actions.push_back({
                static_cast<std::uint16_t>(unsigned_field(action, "code", 127, "monster.action", path)),
                static_cast<std::uint16_t>(unsigned_field(action, "weight", 255, "monster.weight", path))});
            monster.width = static_cast<std::uint16_t>(unsigned_field(source, "width", 256, "monster.width", path));
            monster.height = static_cast<std::uint16_t>(unsigned_field(source, "height", 240, "monster.height", path));
            const auto& colors = list(source, "palettes", 8, path);
            if (colors.empty()) throw std::runtime_error("monster palettes are missing");
            for (const auto& color : colors) {
                const auto expanded = palettes(Json::array({color, color, color, color}), path);
                monster.palettes.push_back(expanded[0]);
            }
            for (const auto& draw : list(source, "draws", 4096, path)) {
                if (!draw.is_array() || draw.size() != 5) throw std::runtime_error("invalid monster tile draw");
                monster.draws.push_back({static_cast<std::uint16_t>(unsigned_value(draw[0], data.tile_count_ - 1U, "monster.tile", path)),
                    static_cast<std::int16_t>(unsigned_value(draw[1], 255, "monster.x", path)),
                    static_cast<std::int16_t>(unsigned_value(draw[2], 239, "monster.y", path)),
                    static_cast<std::uint8_t>(unsigned_value(draw[3], monster.palettes.size() - 1U, "monster.palette", path)),
                    static_cast<std::uint8_t>(unsigned_value(draw[4], 3, "monster.flips", path))});
            }
            data.monsters_.push_back(std::move(monster));
        }
    }
    if (document.contains("shops")) for (const auto& source : list(document, "shops", 256, path)) {
        FieldShop shop;
        shop.map = static_cast<std::uint8_t>(unsigned_field(source, "map", 72, "shop.map", path));
        shop.submap = static_cast<std::uint8_t>(unsigned_field(source, "submap", 31, "shop.submap", path));
        shop.mode = static_cast<std::uint8_t>(unsigned_field(source, "mode", 3, "shop.mode", path));
        if (shop.mode == 0) throw std::runtime_error("shop mode cannot be zero");
        for (const auto& value : list(source, "items", 8, path)) {
            const auto id = static_cast<std::uint16_t>(unsigned_value(value, 126, "shop.item", path));
            const auto item = std::ranges::find_if(data.items_, [&](const auto& candidate) { return candidate.id == id; });
            if (item == data.items_.end() || item->name.empty()) throw std::runtime_error("shop references missing or unnamed item");
            shop.items.push_back(id);
        }
        if (shop.items.empty()) throw std::runtime_error("shop stock cannot be empty");
        data.shops_.push_back(std::move(shop));
    }
    for (const auto& source : list(document, "sprites", 256, path)) {
        FieldSprite sprite;
        sprite.id = static_cast<std::uint16_t>(unsigned_field(source, "id", 255, "sprite.id", path));
        if (!sprite_ids.insert(sprite.id).second) throw std::runtime_error("duplicate field sprite");
        const auto& frames = list(source, "frames", 8, path);
        if (frames.size() != 8) throw std::runtime_error("field sprite requires eight directional frames");
        for (std::size_t frame = 0; frame < 8; ++frame) {
            sprite.frames[frame].quadrants = quadrants(frames[frame], data.tile_count_, path);
            sprite.frames[frame].palette = static_cast<std::uint8_t>(unsigned_field(frames[frame], "palette", 3, "sprite.palette", path));
            const auto& flips = list(frames[frame], "flip_h", 4, path);
            if (flips.size() != 4) throw std::runtime_error("field sprite flips require four values");
            for (std::size_t quadrant = 0; quadrant < 4; ++quadrant) {
                if (!flips[quadrant].is_boolean()) throw std::runtime_error("field sprite flips must be Boolean");
                sprite.frames[frame].flip_horizontal[quadrant] = flips[quadrant].get<bool>();
            }
        }
        data.sprites_.push_back(std::move(sprite));
    }
    std::set<std::pair<std::uint8_t, std::uint8_t>> map_ids;
    for (const auto& source : list(document, "maps", 512, path)) {
        FieldMap map;
        map.id = static_cast<std::uint8_t>(unsigned_field(source, "map", 255, "map", path));
        map.submap = static_cast<std::uint8_t>(unsigned_field(source, "submap", 31, "submap", path));
        if (!map_ids.insert({map.id, map.submap}).second) throw std::runtime_error("duplicate field map");
        map.width = static_cast<std::uint16_t>(unsigned_field(source, "width", 1024, "width", path));
        map.height = static_cast<std::uint16_t>(unsigned_field(source, "height", 1024, "height", path));
        if (map.width == 0 || map.height == 0 || static_cast<std::uint32_t>(map.width) * map.height > 65536) throw std::runtime_error("invalid field dimensions");
        map.border = static_cast<std::uint8_t>(unsigned_field(source, "border", 47, "border", path));
        for (const auto& cell : list(source, "cells", 65536, path)) map.cells.push_back(static_cast<std::uint8_t>(unsigned_value(cell, 255, "cell", path)));
        if (map.cells.size() != static_cast<std::size_t>(map.width) * map.height) throw std::runtime_error("field cells do not match dimensions");
        if (!source.contains("palettes")) throw std::runtime_error("field palettes are missing");
        map.palettes = palettes(source.at("palettes"), path);
        for (const auto& tile : list(source, "tiles", 48, path)) {
            map.tiles.push_back({quadrants(tile, data.tile_count_, path),
                static_cast<std::uint8_t>(unsigned_field(tile, "palette", 3, "tile.palette", path)),
                static_cast<std::uint8_t>(unsigned_field(tile, "behavior", 255, "tile.behavior", path))});
        }
        if (map.tiles.size() < 32 || map.border >= map.tiles.size()) throw std::runtime_error("field tile table is incomplete");
        std::set<std::uint16_t> actor_ids;
        for (const auto& actor : list(source, "actors", 128, path)) {
            FieldActor value;
            value.id = static_cast<std::uint16_t>(unsigned_field(actor, "id", 65535, "actor.id", path));
            if (!actor_ids.insert(value.id).second) throw std::runtime_error("duplicate field actor");
            value.sprite = static_cast<std::uint16_t>(unsigned_field(actor, "sprite", 255, "actor.sprite", path));
            if (!sprite_ids.contains(value.sprite)) throw std::runtime_error("field actor references missing sprite");
            value.message = static_cast<std::uint16_t>(unsigned_field(actor, "message", 65535, "actor.message", path));
            value.x = static_cast<std::int16_t>(unsigned_field(actor, "x", map.width - 1U, "actor.x", path));
            value.y = static_cast<std::int16_t>(unsigned_field(actor, "y", map.height - 1U, "actor.y", path));
            value.facing = static_cast<std::uint8_t>(unsigned_field(actor, "facing", 3, "actor.facing", path));
            value.motion = static_cast<std::uint8_t>(unsigned_field(actor, "motion", 15, "actor.motion", path));
            if (!actor.contains("script")) throw std::runtime_error("field actor script is missing");
            const auto& script = actor.at("script");
            for (const auto& code : list(script, "bytes", 256, path)) value.script.push_back(static_cast<std::uint8_t>(unsigned_value(code, 255, "actor.script", path)));
            if (actor.contains("patrol")) for (const auto& point : list(actor, "patrol", 64, path)) {
                if (!point.is_array() || point.size() != 2) throw std::runtime_error("invalid actor patrol coordinate");
                value.patrol.push_back({static_cast<std::int16_t>(unsigned_value(point[0], map.width - 1U, "patrol.x", path)),
                                       static_cast<std::int16_t>(unsigned_value(point[1], map.height - 1U, "patrol.y", path))});
            }
            if (actor.contains("follow_player")) {
                if (!actor.at("follow_player").is_boolean()) throw std::runtime_error("actor follow-player state must be Boolean");
                value.follow_player = actor.at("follow_player").get<bool>();
            }
            if (actor.contains("remove_at_end")) {
                if (!actor.at("remove_at_end").is_boolean()) throw std::runtime_error("actor removal state must be Boolean");
                value.remove_at_end = actor.at("remove_at_end").get<bool>();
            }
            if (actor.contains("interaction")) {
                const auto& interaction = actor.at("interaction");
                value.interaction = effects(list(interaction, "steps", 256, path), path);
            }
            map.actors.push_back(std::move(value));
        }
        if (source.contains("connections")) for (const auto& connection : list(source, "connections", 256, path)) {
            map.connections.push_back({static_cast<std::int16_t>(unsigned_field(connection, "x", map.width - 1U, "connection.x", path)),
                static_cast<std::int16_t>(unsigned_field(connection, "y", map.height - 1U, "connection.y", path)),
                static_cast<std::uint8_t>(unsigned_field(connection, "map", 255, "connection.map", path)),
                static_cast<std::uint8_t>(unsigned_field(connection, "submap", 31, "connection.submap", path)),
                static_cast<std::int16_t>(unsigned_field(connection, "destination_x", 1023, "connection.destination_x", path)),
                static_cast<std::int16_t>(unsigned_field(connection, "destination_y", 1023, "connection.destination_y", path))});
        }
        if (source.contains("boundary_destination")) {
            const auto& connection = source.at("boundary_destination");
            map.boundary_destination = FieldConnection{0, 0,
                static_cast<std::uint8_t>(unsigned_field(connection, "map", 255, "boundary.map", path)),
                static_cast<std::uint8_t>(unsigned_field(connection, "submap", 31, "boundary.submap", path)),
                static_cast<std::int16_t>(unsigned_field(connection, "x", 1023, "boundary.x", path)),
                static_cast<std::int16_t>(unsigned_field(connection, "y", 1023, "boundary.y", path))};
        }
        data.maps_.push_back(std::move(map));
    }
    if (data.maps_.empty() || data.sprites_.empty()) throw std::runtime_error("field data cannot be empty");
    for (const auto& map : data.maps_) for (const auto& connection : map.connections) {
        const auto& destination = data.map(connection.map, connection.submap);
        if (connection.destination_x >= destination.width || connection.destination_y >= destination.height) throw std::runtime_error("field connection destination is outside its map");
    }
    return data;
}

const FieldMap& FieldData::map(const std::uint8_t id, const std::uint8_t submap) const
{
    const auto found = std::ranges::find_if(maps_, [&](const auto& value) { return value.id == id && value.submap == submap; });
    if (found == maps_.end()) throw std::out_of_range("field map does not exist");
    return *found;
}

const FieldSprite& FieldData::sprite(const std::uint16_t id) const
{
    const auto found = std::ranges::find_if(sprites_, [&](const auto& value) { return value.id == id; });
    if (found == sprites_.end()) throw std::out_of_range("field sprite does not exist");
    return *found;
}

std::uint16_t FieldData::tile_count() const noexcept { return tile_count_; }
const std::filesystem::path& FieldData::image() const noexcept { return image_; }
const std::array<FieldPalette, 4>& FieldData::sprite_palettes() const noexcept { return sprite_palettes_; }
const std::vector<FieldMap>& FieldData::maps() const noexcept { return maps_; }
const std::vector<FieldGrowth>& FieldData::progression() const noexcept { return progression_; }
const std::vector<FieldItem>& FieldData::items() const noexcept { return items_; }
const std::vector<FieldShop>& FieldData::shops() const noexcept { return shops_; }
const std::vector<FieldMonster>& FieldData::monsters() const noexcept { return monsters_; }
const FieldEncounters& FieldData::encounters() const noexcept { return encounters_; }
const std::vector<FieldTreasure>& FieldData::treasures() const noexcept { return treasures_; }
const std::vector<FieldInn>& FieldData::inns() const noexcept { return inns_; }

} // namespace dw4::content