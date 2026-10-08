#include "GraphicsRenderer.hpp"

#include <dw4/game/Game.hpp>

#include <SDL3_image/SDL_image.h>

#include <array>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace dw4::desktop {
namespace {

void require(const bool success)
{
    if (!success) {
        throw std::runtime_error(SDL_GetError());
    }
}

} // namespace

GraphicsRenderer::GraphicsRenderer(SDL_Renderer* renderer, const GraphicsAssets& assets)
    : renderer_(renderer), assets_(assets),
      target_(SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET,
                                game::logical_width, game::logical_height), SDL_DestroyTexture)
{
    if (!target_) {
        throw std::runtime_error(SDL_GetError());
    }
    require(SDL_SetTextureScaleMode(target_.get(), SDL_SCALEMODE_NEAREST));
    require(SDL_SetTextureBlendMode(target_.get(), SDL_BLENDMODE_NONE));
}

void GraphicsRenderer::begin(const game::Color background)
{
    require(SDL_SetRenderTarget(renderer_, target_.get()));
    require(SDL_SetRenderLogicalPresentation(renderer_, game::logical_width, game::logical_height, SDL_LOGICAL_PRESENTATION_DISABLED));
    require(SDL_SetRenderClipRect(renderer_, nullptr));
    require(SDL_SetRenderDrawColor(renderer_, background.red, background.green, background.blue, background.alpha));
    require(SDL_RenderClear(renderer_));
}

SDL_Texture* GraphicsRenderer::texture_for(const game::TileDraw& command)
{
    for (const auto& cached : textures_) {
        if (cached.kind == command.kind && cached.sheet == command.sheet && cached.palette == command.palette) {
            return cached.texture.get();
        }
    }
    if (textures_.size() >= 64) {
        textures_.erase(textures_.begin());
    }
    const auto& images = command.kind == game::GraphicKind::font ? assets_.fonts : assets_.tiles;
    if (command.sheet >= images.size()) {
        throw std::out_of_range("render command references an unknown image sheet");
    }
    const auto& image = images[command.sheet];
    using Surface = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>;
    using Palette = std::unique_ptr<SDL_Palette, decltype(&SDL_DestroyPalette)>;
    Palette palette(SDL_CreatePalette(4), SDL_DestroyPalette);
    if (!palette) {
        throw std::runtime_error(SDL_GetError());
    }
    std::array<SDL_Color, 4> colors{};
    for (std::size_t index = 0; index < colors.size(); ++index) {
        const auto& color = command.palette[index];
        colors[index] = {color.red, color.green, color.blue, color.alpha};
    }
    require(SDL_SetPaletteColors(palette.get(), colors.data(), 0, static_cast<int>(colors.size())));
    Surface indexed(SDL_CreateSurface(static_cast<int>(image.width), static_cast<int>(image.height), SDL_PIXELFORMAT_INDEX8),
                    SDL_DestroySurface);
    if (!indexed) {
        throw std::runtime_error(SDL_GetError());
    }
    require(SDL_SetSurfacePalette(indexed.get(), palette.get()));
    for (std::uint32_t row = 0; row < image.height; ++row) {
        auto* destination = static_cast<std::uint8_t*>(indexed->pixels) + static_cast<std::size_t>(row) * indexed->pitch;
        std::memcpy(destination, image.pixels.data() + static_cast<std::size_t>(row) * image.width, image.width);
    }
    Texture texture(SDL_CreateTextureFromSurface(renderer_, indexed.get()), SDL_DestroyTexture);
    if (!texture) {
        throw std::runtime_error(SDL_GetError());
    }
    require(SDL_SetTextureScaleMode(texture.get(), SDL_SCALEMODE_NEAREST));
    require(SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_BLEND));
    auto* result = texture.get();
    textures_.push_back({command.kind, command.sheet, command.palette, std::move(texture)});
    return result;
}

void GraphicsRenderer::draw(const game::TileDraw& command)
{
    if (command.clip.width <= 0 || command.clip.height <= 0 || command.clip.x < 0 || command.clip.y < 0 ||
        command.clip.width > 256 || command.clip.height > 240 || command.clip.x > 256 - command.clip.width ||
        command.clip.y > 240 - command.clip.height) {
        throw std::invalid_argument("render clip rectangle must fit the logical framebuffer");
    }
    const SDL_Rect clip{command.clip.x, command.clip.y, command.clip.width, command.clip.height};
    require(SDL_SetRenderClipRect(renderer_, &clip));
    const auto& images = command.kind == game::GraphicKind::font ? assets_.fonts : assets_.tiles;
    if (command.sheet >= images.size()) {
        throw std::out_of_range("render command references an unknown image sheet");
    }
    const auto& image = images[command.sheet];
    const auto columns = image.width / 8U;
    if (command.tile >= columns * (image.height / 8U)) {
        throw std::out_of_range("render command tile lies outside its image sheet");
    }
    const SDL_FRect source{static_cast<float>((command.tile % columns) * 8U), static_cast<float>((command.tile / columns) * 8U), 8, 8};
    const SDL_FRect destination{static_cast<float>(command.x), static_cast<float>(command.y), 8, 8};
    SDL_FlipMode flip = SDL_FLIP_NONE;
    if (command.flip_horizontal) flip = static_cast<SDL_FlipMode>(flip | SDL_FLIP_HORIZONTAL);
    if (command.flip_vertical) flip = static_cast<SDL_FlipMode>(flip | SDL_FLIP_VERTICAL);
    require(SDL_RenderTextureRotated(renderer_, texture_for(command), &source, &destination, 0, nullptr, flip));
}

void GraphicsRenderer::finish()
{
    require(SDL_SetRenderTarget(renderer_, nullptr));
    require(SDL_SetRenderLogicalPresentation(renderer_, game::logical_width, game::logical_height, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE));
    require(SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255));
    require(SDL_RenderClear(renderer_));
    require(SDL_RenderTexture(renderer_, target_.get(), nullptr, nullptr));
    require(SDL_RenderPresent(renderer_));
}

void GraphicsRenderer::save_png(const std::filesystem::path& path)
{
    auto* previous = SDL_GetRenderTarget(renderer_);
    require(SDL_SetRenderTarget(renderer_, target_.get()));
    require(SDL_SetRenderClipRect(renderer_, nullptr));
    using Surface = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>;
    Surface surface(SDL_RenderReadPixels(renderer_, nullptr), SDL_DestroySurface);
    require(SDL_SetRenderTarget(renderer_, previous));
    if (!surface) {
        throw std::runtime_error(SDL_GetError());
    }
    const auto utf8_path = path.u8string();
    require(IMG_SavePNG(surface.get(), reinterpret_cast<const char*>(utf8_path.c_str())));
}

} // namespace dw4::desktop