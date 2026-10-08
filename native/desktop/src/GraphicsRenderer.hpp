#pragma once

#include "GraphicsAssets.hpp"

#include <dw4/game/Render.hpp>

#include <SDL3/SDL.h>

#include <filesystem>
#include <memory>
#include <vector>

namespace dw4::desktop {

class GraphicsRenderer final {
public:
    GraphicsRenderer(SDL_Renderer* renderer, const GraphicsAssets& assets);
    void begin(game::Color background);
    void draw(const game::TileDraw& command);
    void finish();
    void save_png(const std::filesystem::path& path);

private:
    using Texture = std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)>;
    struct CachedTexture final {
        game::GraphicKind kind;
        std::uint32_t sheet;
        game::Palette palette;
        Texture texture;
    };

    [[nodiscard]] SDL_Texture* texture_for(const game::TileDraw& command);
    SDL_Renderer* renderer_;
    const GraphicsAssets& assets_;
    Texture target_;
    std::vector<CachedTexture> textures_;
};

} // namespace dw4::desktop