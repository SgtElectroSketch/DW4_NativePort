#pragma once

#include <SDL3/SDL.h>

#include <filesystem>
#include <memory>

namespace dw4::desktop {

class TitleAudio final {
public:
    void play(const std::filesystem::path& path);
    void stop() noexcept;

private:
    using Stream = std::unique_ptr<SDL_AudioStream, decltype(&SDL_DestroyAudioStream)>;
    Stream stream_{nullptr, SDL_DestroyAudioStream};
};

} // namespace dw4::desktop