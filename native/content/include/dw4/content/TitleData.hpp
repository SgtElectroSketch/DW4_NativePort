#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace dw4::content {

using TitleColor = std::array<std::uint8_t, 4>;
using TitlePalette = std::array<TitleColor, 4>;

struct TitleTile final {
    std::uint16_t tile{};
    std::int16_t x{};
    std::int16_t y{};
    std::uint16_t palette{};
    std::uint8_t flips{};
    std::uint16_t clip_top{};
    std::uint16_t clip_height{};
};

struct TitleFrame final {
    TitleColor background;
    std::vector<TitleTile> draws;
};

struct TitleMusic final {
    std::uint32_t frame{};
    std::uint16_t track{};
};

struct TitleData final {
    std::filesystem::path image;
    std::uint32_t tiles{};
    std::uint32_t artwork_frame{};
    std::uint32_t fade_frames{};
    std::uint32_t loop_start{};
    std::uint32_t loop_frames{};
    std::vector<TitlePalette> palettes;
    std::vector<TitleFrame> frames;
    std::vector<TitleFrame> exit_frames;
    std::vector<TitleMusic> music;

    [[nodiscard]] static TitleData load(const std::filesystem::path& directory);
};

} // namespace dw4::content