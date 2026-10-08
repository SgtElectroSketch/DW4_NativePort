#include "TitleAudio.hpp"

#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace dw4::desktop {

void TitleAudio::play(const std::filesystem::path& path)
{
    stop();
    SDL_AudioSpec format{};
    Uint8* raw = nullptr;
    Uint32 length = 0;
    const auto utf8_path = path.u8string();
    if (!SDL_LoadWAV(reinterpret_cast<const char*>(utf8_path.c_str()), &format, &raw, &length)) {
        throw std::runtime_error("cannot load title music: " + path.string() + ": " + SDL_GetError());
    }
    const std::unique_ptr<Uint8, decltype(&SDL_free)> samples(raw, SDL_free);
    if (length == 0 || length > static_cast<Uint32>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("title WAV length is unsupported");
    }
    Stream stream(SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &format, nullptr, nullptr), SDL_DestroyAudioStream);
    if (!stream || !SDL_PutAudioStreamData(stream.get(), samples.get(), static_cast<int>(length)) ||
        !SDL_FlushAudioStream(stream.get()) || !SDL_ResumeAudioStreamDevice(stream.get())) {
        throw std::runtime_error("cannot play title music: " + std::string(SDL_GetError()));
    }
    stream_ = std::move(stream);
}

void TitleAudio::stop() noexcept
{
    stream_.reset();
}

} // namespace dw4::desktop