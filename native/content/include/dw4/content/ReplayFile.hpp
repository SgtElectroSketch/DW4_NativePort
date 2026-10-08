#pragma once

#include <cstdint>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

namespace dw4::content {

inline constexpr std::uint32_t replay_file_schema_version = 1;
inline constexpr std::uint32_t replay_file_max_frames = 360000;
inline constexpr std::uint32_t replay_file_max_draws = 1000000;

struct ReplaySnapshotRecord final {
    std::uint64_t frame{};
    std::uint8_t input{};
    std::uint8_t pressed{};
    std::uint8_t released{};
    std::mt19937 random_engine{std::mt19937::default_seed};
    std::uint64_t random_draw_counter{};
};

struct ReplayInputRecord final {
    std::uint8_t input{};
    std::uint32_t random_draws{};
};

struct ReplayCheckpointRecord final {
    std::uint32_t frame_index{};
    ReplaySnapshotRecord snapshot;
};

struct ReplayDocument final {
    ReplaySnapshotRecord initial;
    std::vector<ReplayInputRecord> frames;
    std::vector<ReplayCheckpointRecord> checkpoints;
};

class ReplayFile final {
public:
    [[nodiscard]] static ReplayDocument load(const std::filesystem::path& path);
    static void save(const std::filesystem::path& path, const ReplayDocument& document);
    [[nodiscard]] static std::string rng_compatibility();
};

} // namespace dw4::content