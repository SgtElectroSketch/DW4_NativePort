#include "GraphicsAssets.hpp"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace dw4::desktop {
namespace {

using Surface = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>;

class SurfaceLock final {
public:
    explicit SurfaceLock(SDL_Surface* surface)
        : surface_(surface)
    {
        if (!SDL_LockSurface(surface_)) {
            throw std::runtime_error(SDL_GetError());
        }
    }
    ~SurfaceLock() { SDL_UnlockSurface(surface_); }
    SurfaceLock(const SurfaceLock&) = delete;
    SurfaceLock& operator=(const SurfaceLock&) = delete;

private:
    SDL_Surface* surface_;
};

std::filesystem::path contained_image(const std::filesystem::path& root, const std::filesystem::path& relative)
{
    const auto directory = std::filesystem::weakly_canonical(root);
    const auto image = std::filesystem::weakly_canonical(directory / relative);
    const auto within = image.lexically_relative(directory);
    if (within.empty() || within.has_root_path()) {
        throw std::runtime_error("graphics image path escapes its asset directory");
    }
    for (const auto& component : within) {
        if (component == "..") {
            throw std::runtime_error("graphics image path escapes its asset directory");
        }
    }
    return image;
}

IndexedImage decode(const std::filesystem::path& path, const content::TilePixelFormat format)
{
    const auto utf8_path = path.u8string();
    Surface source(IMG_Load(reinterpret_cast<const char*>(utf8_path.c_str())), SDL_DestroySurface);
    if (!source) {
        throw std::runtime_error("cannot decode graphics PNG: " + path.string() + ": " + SDL_GetError());
    }
    if (source->w != 128 || source->h <= 0 || source->h % 8 != 0 || source->h > 32768) {
        throw std::runtime_error("graphics sheet must be 128 pixels wide with bounded 8-pixel rows: " + path.string());
    }
    Surface rgba(SDL_ConvertSurface(source.get(), SDL_PIXELFORMAT_RGBA32), SDL_DestroySurface);
    if (!rgba) {
        throw std::runtime_error("cannot convert graphics PNG: " + path.string() + ": " + SDL_GetError());
    }
    IndexedImage image{static_cast<std::uint32_t>(rgba->w), static_cast<std::uint32_t>(rgba->h), {}};
    image.pixels.resize(static_cast<std::size_t>(image.width) * image.height);
    const SurfaceLock lock(rgba.get());
    for (std::uint32_t row = 0; row < image.height; ++row) {
        const auto* pixels = static_cast<const std::uint8_t*>(rgba->pixels) + static_cast<std::size_t>(row) * rgba->pitch;
        for (std::uint32_t column = 0; column < image.width; ++column) {
            const auto* color = pixels + column * 4U;
            if (color[0] != color[1] || color[0] != color[2] || color[3] != 255 ||
                (format == content::TilePixelFormat::monochrome ? (color[0] != 0 && color[0] != 255) : color[0] % 85 != 0)) {
                throw std::runtime_error("graphics PNG does not match its neutral indexed palette: " + path.string());
            }
            image.pixels[static_cast<std::size_t>(row) * image.width + column] =
                static_cast<std::uint8_t>(format == content::TilePixelFormat::monochrome ? color[0] / 255 : color[0] / 85);
        }
    }
    return image;
}

} // namespace

GraphicsAssets GraphicsAssets::load(const content::AssetCatalog& catalog)
{
    GraphicsAssets assets;
    const auto root = catalog.root() / "graphics";
    for (const auto& sheet : catalog.tile_sheets()) {
        auto image = decode(contained_image(root / "chr", sheet.image), sheet.format);
        if (image.height != ((sheet.tile_count + 15U) / 16U) * 8U) {
            throw std::runtime_error("tile sheet dimensions disagree with declared tile count: " + sheet.image.string());
        }
        assets.tiles.push_back(std::move(image));
    }
    for (const auto& font : catalog.font_sheets()) {
        auto image = decode(contained_image(root, font.image), content::TilePixelFormat::monochrome);
        const auto tile_capacity = (image.width / 8U) * (image.height / 8U);
        for (const auto& glyph : font.glyphs) {
            if (glyph.tile >= tile_capacity) {
                throw std::runtime_error("font glyph lies outside its image: " + font.image.string());
            }
        }
        assets.fonts.push_back(std::move(image));
    }
    return assets;
}

IndexedImage GraphicsAssets::load_title_patterns(const std::filesystem::path& path, const std::uint32_t tile_count)
{
    auto image = decode(path, content::TilePixelFormat::indexed_four_color);
    if (tile_count == 0 || image.height != ((tile_count + 15U) / 16U) * 8U) {
        throw std::runtime_error("title pattern dimensions disagree with declared count: " + path.string());
    }
    return image;
}

} // namespace dw4::desktop