#pragma once

#include <cstddef>
#include <filesystem>

namespace dw4::content {

struct AssetSummary final {
    std::size_t dialogue_messages{};
    std::size_t locations{};
    std::size_t field_sprite_sets{};
    std::size_t monsters{};
    std::size_t music_tracks{};
    std::size_t sound_effects{};
};

class AssetCatalog final {
public:
    [[nodiscard]] static AssetCatalog load(const std::filesystem::path& root);

    [[nodiscard]] const std::filesystem::path& root() const noexcept;
    [[nodiscard]] const AssetSummary& summary() const noexcept;

private:
    std::filesystem::path root_;
    AssetSummary summary_;
};

} // namespace dw4::content