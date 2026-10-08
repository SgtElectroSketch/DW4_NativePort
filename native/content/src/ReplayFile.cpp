#include <dw4/content/AssetCatalog.hpp>
#include <dw4/content/ReplayFile.hpp>

#include "JsonValidation.hpp"

#include <fstream>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace dw4::content {
namespace {

using detail::Json;
using detail::unsigned_field;
constexpr std::uintmax_t maximum_file_bytes = 64U * 1024U * 1024U;
constexpr const char* rules_version = "foundation-1";

const Json& required(const Json& record, const char* key, const std::filesystem::path& path)
{
    const auto entry = record.find(key);
    if (entry == record.end()) {
        throw std::runtime_error("missing replay field '" + std::string(key) + "': " + path.string());
    }
    return *entry;
}

ReplaySnapshotRecord read_snapshot(const Json& document, const std::string& field, const std::filesystem::path& path)
{
    if (!document.is_object()) {
        throw std::runtime_error("replay snapshot '" + field + "' must be an object: " + path.string());
    }
    ReplaySnapshotRecord snapshot;
    snapshot.frame = unsigned_field(document, "frame", std::numeric_limits<std::uint64_t>::max(), field + ".frame", path);
    snapshot.input = static_cast<std::uint8_t>(unsigned_field(document, "input", 255, field + ".input", path));
    snapshot.pressed = static_cast<std::uint8_t>(unsigned_field(document, "pressed", 255, field + ".pressed", path));
    snapshot.released = static_cast<std::uint8_t>(unsigned_field(document, "released", 255, field + ".released", path));
    snapshot.random_draw_counter = unsigned_field(document, "random_draw_counter", std::numeric_limits<std::uint64_t>::max(),
                                                  field + ".random_draw_counter", path);
    if ((snapshot.pressed & ~snapshot.input) != 0 || (snapshot.released & snapshot.input) != 0) {
        throw std::runtime_error("inconsistent replay button edges in '" + field + "': " + path.string());
    }
    const auto& state = required(document, "random_engine", path);
    if (!state.is_string() || state.get_ref<const std::string&>().size() > 16384) {
        throw std::runtime_error("invalid replay random_engine field: " + path.string());
    }
    std::istringstream stream(state.get<std::string>());
    stream.imbue(std::locale::classic());
    try {
        stream >> snapshot.random_engine;
    } catch (const std::exception&) {
        throw std::runtime_error("invalid replay random_engine state: " + path.string());
    }
    if (!stream) {
        throw std::runtime_error("invalid replay random_engine state: " + path.string());
    }
    stream >> std::ws;
    if (!stream.eof()) {
        throw std::runtime_error("trailing replay random_engine data: " + path.string());
    }
    return snapshot;
}

Json write_snapshot(const ReplaySnapshotRecord& snapshot)
{
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << snapshot.random_engine;
    return Json{{"frame", snapshot.frame}, {"input", snapshot.input}, {"pressed", snapshot.pressed},
                {"released", snapshot.released}, {"random_engine", stream.str()},
                {"random_draw_counter", snapshot.random_draw_counter}};
}

ReplayDocument read_document(const Json& document, const std::filesystem::path& path)
{
    if (!document.is_object()) {
        throw std::runtime_error("replay root must be an object: " + path.string());
    }
    if (unsigned_field(document, "schema_version", std::numeric_limits<std::uint32_t>::max(), "schema_version", path) !=
        replay_file_schema_version || required(document, "rules_version", path) != rules_version ||
        required(document, "rng_compatibility", path) != ReplayFile::rng_compatibility() ||
        unsigned_field(document, "asset_schema_version", std::numeric_limits<std::uint32_t>::max(), "asset_schema_version", path) !=
        asset_catalog_schema_version) {
        throw std::runtime_error("unsupported replay format, rules, assets, or RNG compatibility: " + path.string());
    }
    ReplayDocument result;
    result.initial = read_snapshot(required(document, "initial", path), "initial", path);
    const auto& frames = required(document, "frames", path);
    const auto& checkpoints = required(document, "checkpoints", path);
    if (!frames.is_array() || frames.size() > replay_file_max_frames || !checkpoints.is_array() ||
        checkpoints.size() > frames.size()) {
        throw std::runtime_error("invalid or excessive replay frames/checkpoints: " + path.string());
    }
    if (result.initial.frame > std::numeric_limits<std::uint64_t>::max() - frames.size()) {
        throw std::runtime_error("replay frame counter would overflow: " + path.string());
    }
    result.frames.reserve(frames.size());
    std::uint64_t total_draws = 0;
    for (std::size_t index = 0; index < frames.size(); ++index) {
        const auto& frame = frames[index];
        const auto field = "frames[" + std::to_string(index) + "]";
        if (!frame.is_object()) {
            throw std::runtime_error("replay frame must be an object: " + field + ": " + path.string());
        }
        const auto buttons = unsigned_field(frame, "input", 255, field + ".input", path);
        const auto draws = unsigned_field(frame, "random_draws", replay_file_max_draws, field + ".random_draws", path);
        total_draws += draws;
        if (total_draws > replay_file_max_draws) {
            throw std::runtime_error("replay exceeds maximum random draws: " + path.string());
        }
        result.frames.push_back({static_cast<std::uint8_t>(buttons), static_cast<std::uint32_t>(draws)});
    }
    if (result.initial.random_draw_counter > std::numeric_limits<std::uint64_t>::max() - total_draws) {
        throw std::runtime_error("replay random draw counter would overflow: " + path.string());
    }
    result.checkpoints.reserve(checkpoints.size());
    for (std::size_t index = 0; index < checkpoints.size(); ++index) {
        const auto& checkpoint = checkpoints[index];
        if (!checkpoint.is_object()) {
            throw std::runtime_error("replay checkpoint must be an object: " + path.string());
        }
        const auto frame_index = unsigned_field(checkpoint, "frame_index", replay_file_max_frames, "checkpoint.frame_index", path);
        if (frame_index >= frames.size() || (index > 0 && frame_index <= result.checkpoints.back().frame_index)) {
            throw std::runtime_error("replay checkpoint indexes must be in range and strictly increasing: " + path.string());
        }
        result.checkpoints.push_back({static_cast<std::uint32_t>(frame_index),
                                      read_snapshot(required(checkpoint, "snapshot", path), "checkpoint.snapshot", path)});
    }
    return result;
}

Json write_document(const ReplayDocument& document)
{
    Json frames = Json::array();
    for (const auto& frame : document.frames) {
        frames.push_back(Json{{"input", frame.input}, {"random_draws", frame.random_draws}});
    }
    Json checkpoints = Json::array();
    for (const auto& checkpoint : document.checkpoints) {
        checkpoints.push_back(Json{{"frame_index", checkpoint.frame_index}, {"snapshot", write_snapshot(checkpoint.snapshot)}});
    }
    return Json{{"schema_version", replay_file_schema_version}, {"rules_version", rules_version},
                {"rng_compatibility", ReplayFile::rng_compatibility()}, {"asset_schema_version", asset_catalog_schema_version},
                {"initial", write_snapshot(document.initial)}, {"frames", std::move(frames)}, {"checkpoints", std::move(checkpoints)}};
}

} // namespace

std::string ReplayFile::rng_compatibility()
{
#if defined(_MSC_VER) && defined(_MSVC_STL_UPDATE)
    return "std-mt19937-uniform-u32-msvc-" + std::to_string(_MSC_VER) + "-stl-" + std::to_string(_MSVC_STL_UPDATE);
#elif defined(__GLIBCXX__)
    return "std-mt19937-uniform-u32-libstdcxx-" + std::to_string(__GLIBCXX__);
#elif defined(_LIBCPP_VERSION)
    return "std-mt19937-uniform-u32-libcxx-" + std::to_string(_LIBCPP_VERSION);
#else
    throw std::runtime_error("replay persistence requires a recognized standard-library compatibility identifier");
#endif
}

ReplayDocument ReplayFile::load(const std::filesystem::path& path)
{
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error || size > maximum_file_bytes) {
        throw std::runtime_error("replay is missing, unreadable, or larger than 64 MiB: " + path.string());
    }
    std::ifstream stream(path);
    if (!stream) {
        throw std::runtime_error("cannot open replay: " + path.string());
    }
    Json document;
    try {
        stream >> document;
    } catch (const Json::parse_error& parse_error) {
        throw std::runtime_error("invalid replay JSON at byte " + std::to_string(parse_error.byte) + ": " + path.string());
    }
    stream >> std::ws;
    if (!stream.eof()) {
        throw std::runtime_error("trailing data after replay JSON: " + path.string());
    }
    return read_document(document, path);
}

void ReplayFile::save(const std::filesystem::path& path, const ReplayDocument& document)
{
    if (document.frames.size() > replay_file_max_frames || document.checkpoints.size() > document.frames.size()) {
        throw std::runtime_error("replay exceeds frame/checkpoint limits: " + path.string());
    }
    const auto json = write_document(document);
    static_cast<void>(read_document(json, path));
    const auto text = json.dump(2) + '\n';
    if (text.size() > maximum_file_bytes) {
        throw std::runtime_error("replay exceeds the 64 MiB limit: " + path.string());
    }
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        throw std::runtime_error("cannot write replay: " + path.string());
    }
    stream << text;
    stream.flush();
    if (!stream) {
        throw std::runtime_error("failed to write complete replay: " + path.string());
    }
}

} // namespace dw4::content