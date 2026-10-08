#pragma once

#include <dw4/content/Graphics.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace dw4::content {

inline constexpr std::uint32_t asset_catalog_schema_version = 1;

struct DialogueMessageId final {
    std::uint16_t value{};

    [[nodiscard]] constexpr bool operator==(const DialogueMessageId&) const noexcept = default;
};

struct DialogueMessage final {
    DialogueMessageId id;
    std::vector<std::uint8_t> symbols;
};

struct AssetSummary final {
    std::size_t dialogue_messages{};
    std::size_t locations{};
    std::size_t field_sprite_sets{};
    std::size_t monsters{};
    std::size_t music_tracks{};
    std::size_t sound_effects{};
    std::size_t chr_sheets{};
    std::size_t fonts{};
    std::size_t reference_scenes{};
};

class AssetCatalog final {
public:
    [[nodiscard]] static AssetCatalog load(const std::filesystem::path& root);

    [[nodiscard]] const std::filesystem::path& root() const noexcept;
    [[nodiscard]] const AssetSummary& summary() const noexcept;
    [[nodiscard]] std::span<const DialogueMessage> dialogue_messages() const noexcept;
    [[nodiscard]] const DialogueMessage& dialogue_message(DialogueMessageId id) const;
    [[nodiscard]] std::span<const TileSheet> tile_sheets() const noexcept;
    [[nodiscard]] std::span<const FontSheet> font_sheets() const noexcept;

private:
    std::filesystem::path root_;
    AssetSummary summary_;
    std::vector<DialogueMessage> dialogue_messages_;
    std::vector<TileSheet> tile_sheets_;
    std::vector<FontSheet> font_sheets_;
};

} // namespace dw4::content