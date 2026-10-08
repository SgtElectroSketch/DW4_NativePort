#pragma once

#include <dw4/content/AssetCatalog.hpp>

#include <cstdint>
#include <vector>

namespace dw4::desktop {

struct IndexedImage final {
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<std::uint8_t> pixels;
};

struct GraphicsAssets final {
    std::vector<IndexedImage> tiles;
    std::vector<IndexedImage> fonts;

    [[nodiscard]] static GraphicsAssets load(const content::AssetCatalog& catalog);
    [[nodiscard]] static IndexedImage load_title_patterns(const std::filesystem::path& path, std::uint32_t tile_count);
};

} // namespace dw4::desktop