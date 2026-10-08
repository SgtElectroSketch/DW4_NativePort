#include <dw4/content/AdventureSave.hpp>
#include <dw4/content/ReplayFile.hpp>

#include "JsonValidation.hpp"

#include <zlib.h>

#include <fstream>
#include <algorithm>
#include <set>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace dw4::content {
namespace {

using detail::Json;
using detail::unsigned_field;
using detail::unsigned_value;

std::uint32_t checksum(const Json& document)
{
    const auto payload = document.dump();
    return static_cast<std::uint32_t>(crc32(0, reinterpret_cast<const Bytef*>(payload.data()), static_cast<uInt>(payload.size())));
}

const Json& collection(const Json& object, const char* key, const std::size_t maximum, const std::filesystem::path& path)
{
    if (!object.is_object() || !object.contains(key) || !object.at(key).is_array() || object.at(key).size() > maximum) {
        throw std::runtime_error("invalid gameplay collection " + std::string(key) + ": " + path.string());
    }
    return object.at(key);
}

std::vector<SavedItem> read_items(const Json& document, const char* key, const std::size_t maximum, const std::filesystem::path& path)
{
    std::vector<SavedItem> items;
    for (const auto& item : collection(document, key, maximum, path)) {
        items.push_back({static_cast<std::uint16_t>(unsigned_field(item, "id", 255, key, path)),
                         static_cast<std::uint8_t>(unsigned_field(item, "equipped", 5, key, path))});
    }
    return items;
}

Json write_items(const std::vector<SavedItem>& items)
{
    auto result = Json::array();
    for (const auto& item : items) result.push_back({{"id", item.id}, {"equipped", item.equipped}});
    return result;
}

SavedWorld read_world(const Json& document, const std::filesystem::path& path)
{
    SavedWorld world;
    const auto number = [&](const Json& object, const char* key, const std::uint64_t maximum) {
        return unsigned_field(object, key, maximum, key, path);
    };
    world.map = static_cast<std::uint8_t>(number(document, "map", 255));
    if (world.map > 72 && world.map != 255) throw std::runtime_error("gameplay map identity is invalid");
    world.submap = static_cast<std::uint8_t>(number(document, "submap", 31));
    world.x = static_cast<std::int16_t>(number(document, "x", 1023));
    world.y = static_cast<std::int16_t>(number(document, "y", 1023));
    world.facing = static_cast<std::uint8_t>(number(document, "facing", 3));
    world.gold = static_cast<std::uint32_t>(number(document, "gold", 9999999));
    world.bank_gold = static_cast<std::uint32_t>(number(document, "bank_gold", 9999999));
    world.casino_coins = static_cast<std::uint32_t>(number(document, "casino_coins", 9999999));
    world.bag = read_items(document, "bag", 256, path);
    const auto& characters = collection(document, "characters", 9, path);
    if (characters.size() != 9) throw std::runtime_error("gameplay save requires all nine character records");
    for (std::size_t index = 0; index < characters.size(); ++index) {
        const auto& source = characters[index];
        auto& target = world.characters[index];
        target.id = static_cast<std::uint8_t>(number(source, "id", 8));
        target.level = static_cast<std::uint8_t>(number(source, "level", 99));
        target.hp = static_cast<std::uint16_t>(number(source, "hp", 999));
        target.maximum_hp = static_cast<std::uint16_t>(number(source, "maximum_hp", 999));
        target.mp = static_cast<std::uint16_t>(number(source, "mp", 999));
        target.maximum_mp = static_cast<std::uint16_t>(number(source, "maximum_mp", 999));
        target.strength = static_cast<std::uint16_t>(number(source, "strength", 255));
        target.agility = static_cast<std::uint16_t>(number(source, "agility", 255));
        target.vitality = static_cast<std::uint16_t>(number(source, "vitality", 255));
        target.intelligence = static_cast<std::uint16_t>(number(source, "intelligence", 255));
        target.luck = static_cast<std::uint16_t>(number(source, "luck", 255));
        target.experience = static_cast<std::uint32_t>(number(source, "experience", 9999999));
        target.conditions = static_cast<std::uint16_t>(number(source, "conditions", 65535));
        target.inventory = read_items(source, "inventory", 8, path);
        for (const auto& spell : collection(source, "spells", 64, path)) {
            target.spells.push_back(static_cast<std::uint16_t>(unsigned_value(spell, 255, "spell", path)));
        }
    }
    const auto& party = collection(document, "party", 4, path);
    if (party.size() != 4) throw std::runtime_error("gameplay save requires four party slots");
    for (std::size_t index = 0; index < party.size(); ++index) world.party[index] = static_cast<std::uint8_t>(unsigned_value(party[index], 255, "party", path));
    for (const auto& flag : collection(document, "story_flags", 1024, path)) world.story_flags.push_back(static_cast<std::uint16_t>(unsigned_value(flag, 65535, "flag", path)));
    for (const auto& source : collection(document, "actors", 128, path)) {
        if (!source.contains("active") || !source.at("active").is_boolean()) throw std::runtime_error("gameplay actor active state must be Boolean");
        world.actors.push_back({static_cast<std::uint16_t>(number(source, "id", 65535)), static_cast<std::uint16_t>(number(source, "sprite", 255)),
            static_cast<std::int16_t>(number(source, "x", 1023)), static_cast<std::int16_t>(number(source, "y", 1023)),
            static_cast<std::int16_t>(number(source, "target_x", 1023)), static_cast<std::int16_t>(number(source, "target_y", 1023)),
            static_cast<std::uint8_t>(number(source, "facing", 3)), static_cast<std::uint8_t>(number(source, "step", 15)),
            static_cast<std::uint16_t>(number(source, "instruction", 65535)), static_cast<std::uint16_t>(number(source, "delay", 65535)),
            source.at("active").get<bool>()});
    }
    world.event_step = static_cast<std::uint16_t>(number(document, "event_step", 65535));
    world.event_delay = static_cast<std::uint16_t>(number(document, "event_delay", 65535));
    world.phase = static_cast<std::uint8_t>(number(document, "phase", 3));
    return world;
}

Json write_world(const SavedWorld& world)
{
    auto characters = Json::array();
    for (const auto& character : world.characters) {
        characters.push_back({{"id", character.id}, {"level", character.level}, {"hp", character.hp}, {"maximum_hp", character.maximum_hp},
            {"mp", character.mp}, {"maximum_mp", character.maximum_mp}, {"strength", character.strength}, {"agility", character.agility},
            {"vitality", character.vitality}, {"intelligence", character.intelligence}, {"luck", character.luck}, {"experience", character.experience},
            {"conditions", character.conditions}, {"inventory", write_items(character.inventory)}, {"spells", character.spells}});
    }
    auto actors = Json::array();
    for (const auto& actor : world.actors) {
        actors.push_back({{"id", actor.id}, {"sprite", actor.sprite}, {"x", actor.x}, {"y", actor.y}, {"target_x", actor.target_x},
            {"target_y", actor.target_y}, {"facing", actor.facing}, {"step", actor.step}, {"instruction", actor.instruction},
            {"delay", actor.delay}, {"active", actor.active}});
    }
    return {{"map", world.map}, {"submap", world.submap}, {"x", world.x}, {"y", world.y}, {"facing", world.facing},
        {"characters", characters}, {"party", world.party}, {"gold", world.gold}, {"bank_gold", world.bank_gold}, {"casino_coins", world.casino_coins},
        {"bag", write_items(world.bag)}, {"story_flags", world.story_flags}, {"actors", actors}, {"event_step", world.event_step},
        {"event_delay", world.event_delay}, {"phase", world.phase}};
}

void validate(const AdventureSave& state)
{
    if (state.name.empty() || state.name.size() > 8 || state.gender > 1 || state.message_speed > 7 || state.chapter < 1 || state.chapter > 5) {
        throw std::runtime_error("adventure save contains invalid name, gender, speed or chapter");
    }
    for (const auto glyph : state.name) {
        if (glyph > 140) {
            throw std::runtime_error("adventure name contains an invalid glyph");
        }
    }
    if (std::ranges::none_of(state.name, [](const std::uint16_t glyph) { return glyph != 0; })) {
        throw std::runtime_error("adventure name must contain a nonblank character");
    }
    if (state.world) {
        const auto& world = *state.world;
        const auto bounded = read_world(write_world(world), {});
        if (bounded != world) throw std::runtime_error("gameplay save contains invalid values");
        for (std::size_t id = 0; id < world.characters.size(); ++id) {
            const auto& character = world.characters[id];
            if (character.id != id || character.level == 0 || character.hp > character.maximum_hp || character.mp > character.maximum_mp) {
                throw std::runtime_error("gameplay character identity or HP/MP bounds are invalid");
            }
        }
        std::set<std::uint8_t> party;
        for (const auto id : world.party) {
            if (id == 255) continue;
            if (id >= world.characters.size() || !party.insert(id).second) throw std::runtime_error("gameplay party contains an invalid or duplicate character");
        }
        if (party.empty()) throw std::runtime_error("gameplay party cannot be empty");
        if (std::set<std::uint16_t>(world.story_flags.begin(), world.story_flags.end()).size() != world.story_flags.size()) throw std::runtime_error("duplicate gameplay story flag");
        std::set<std::uint16_t> actor_ids;
        for (const auto& actor : world.actors) if (!actor_ids.insert(actor.id).second) throw std::runtime_error("duplicate gameplay actor identity");
    }
}

} // namespace

AdventureSave AdventureSaveFile::load(const std::filesystem::path& path)
{
    std::error_code error;
    const auto bytes = std::filesystem::file_size(path, error);
    if (error || bytes > 1024U * 1024U) {
        throw std::runtime_error("adventure log is unreadable or exceeds its size limit: " + path.string());
    }
    std::ifstream stream(path);
    Json document;
    try { stream >> document; }
    catch (const Json::parse_error&) { throw std::runtime_error("adventure log JSON is corrupt: " + path.string()); }
    stream >> std::ws;
    if (!stream.eof() || !document.is_object() || !document.contains("magic") || document.at("magic") != "DW4_NATIVE_ADVENTURE" ||
        !document.contains("rng_compatibility") || document.at("rng_compatibility") != ReplayFile::rng_compatibility() ||
        unsigned_field(document, "version", 2, "save.version", path) == 0 || !document.contains("name") ||
        !document.at("name").is_array() || !document.contains("random_engine") || !document.at("random_engine").is_string()) {
        throw std::runtime_error("adventure log format or version is unsupported: " + path.string());
    }
    const auto stored_checksum = unsigned_field(document, "checksum", std::numeric_limits<std::uint32_t>::max(), "save.checksum", path);
    document.erase("checksum");
    if (checksum(document) != stored_checksum) {
        throw std::runtime_error("adventure log integrity check failed: " + path.string());
    }
    AdventureSave result;
    if (document.at("name").empty() || document.at("name").size() > 8) {
        throw std::runtime_error("adventure name length is invalid: " + path.string());
    }
    for (const auto& glyph : document.at("name")) {
        result.name.push_back(static_cast<std::uint16_t>(unsigned_value(glyph, 140, "save.name", path)));
    }
    result.gender = static_cast<std::uint8_t>(unsigned_field(document, "gender", 1, "save.gender", path));
    result.message_speed = static_cast<std::uint8_t>(unsigned_field(document, "message_speed", 7, "save.message_speed", path));
    result.chapter = static_cast<std::uint8_t>(unsigned_field(document, "chapter", 5, "save.chapter", path));
    result.frame = unsigned_field(document, "frame", std::numeric_limits<std::uint64_t>::max(), "save.frame", path);
    result.random_draws = unsigned_field(document, "random_draws", std::numeric_limits<std::uint64_t>::max(), "save.random_draws", path);
    if (document.at("version") == 2) {
        if (!document.contains("world")) throw std::runtime_error("version 2 adventure requires gameplay state");
        result.world = read_world(document.at("world"), path);
    } else if (document.contains("world")) throw std::runtime_error("version 1 profile cannot contain gameplay state");
    const auto& engine_text = document.at("random_engine").get_ref<const std::string&>();
    if (engine_text.size() > 16384) throw std::runtime_error("adventure RNG state is oversized");
    std::istringstream engine(engine_text);
    engine.imbue(std::locale::classic());
    try { engine >> result.random_engine; }
    catch (const std::exception&) { throw std::runtime_error("adventure RNG state is corrupt"); }
    if (!engine) throw std::runtime_error("adventure RNG state is corrupt");
    engine >> std::ws;
    if (!engine.eof()) throw std::runtime_error("adventure RNG state contains trailing data");
    validate(result);
    return result;
}

void AdventureSaveFile::save(const std::filesystem::path& path, const AdventureSave& state)
{
    validate(state);
    std::ostringstream engine;
    engine.imbue(std::locale::classic());
    engine << state.random_engine;
    Json document{{"magic", "DW4_NATIVE_ADVENTURE"}, {"version", state.world ? 2 : 1}, {"rng_compatibility", ReplayFile::rng_compatibility()},
                        {"name", state.name}, {"gender", state.gender}, {"message_speed", state.message_speed},
                        {"chapter", state.chapter}, {"frame", state.frame}, {"random_engine", engine.str()}, {"random_draws", state.random_draws}};
    if (state.world) document["world"] = write_world(*state.world);
    document["checksum"] = checksum(document);
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) throw std::runtime_error("cannot write adventure log: " + path.string());
    stream << document.dump(2) << '\n';
    stream.flush();
    if (!stream) throw std::runtime_error("cannot complete adventure log write: " + path.string());
}

} // namespace dw4::content