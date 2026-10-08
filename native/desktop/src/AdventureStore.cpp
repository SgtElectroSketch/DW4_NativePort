#include "AdventureStore.hpp"

#include <Windows.h>

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace dw4::desktop {

AdventureStore::AdventureStore(std::filesystem::path directory)
    : directory_(std::filesystem::absolute(std::move(directory)).lexically_normal())
{
    std::filesystem::create_directories(directory_);
}

std::filesystem::path AdventureStore::path(const std::size_t slot) const
{
    if (slot >= 3) throw std::out_of_range("adventure log slot must be 0 through 2");
    return directory_ / ("adventure-" + std::to_string(slot + 1) + ".json");
}

std::array<AdventureSlot, 3> AdventureStore::inspect() const
{
    std::array<AdventureSlot, 3> slots;
    for (std::size_t slot = 0; slot < slots.size(); ++slot) {
        if (!std::filesystem::exists(path(slot))) continue;
        try { slots[slot].save = content::AdventureSaveFile::load(path(slot)); }
        catch (const std::exception& error) { slots[slot].error = error.what(); }
    }
    return slots;
}

void AdventureStore::write(const std::size_t slot, const content::AdventureSave& save)
{
    const auto destination = path(slot);
    static std::atomic<std::uint64_t> sequence{};
    const auto suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
                        std::to_string(sequence.fetch_add(1));
    auto temporary = destination;
    temporary += "." + suffix + ".tmp";
    try {
        content::AdventureSaveFile::save(temporary, save);
        if (content::AdventureSaveFile::load(temporary) != save) {
            throw std::runtime_error("adventure save validation failed before replacement");
        }
        const auto handle = CreateFileW(temporary.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                        FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) throw std::system_error(GetLastError(), std::system_category(), "open save for flush");
        const auto flushed = FlushFileBuffers(handle);
        const auto flush_error = GetLastError();
        CloseHandle(handle);
        if (!flushed) throw std::system_error(flush_error, std::system_category(), "flush adventure save");
        bool replaced = false;
        if (std::filesystem::exists(destination)) {
            auto backup = destination;
            backup += ".backup";
            if (std::filesystem::exists(backup)) std::filesystem::remove(backup);
            replaced = ReplaceFileW(destination.c_str(), temporary.c_str(), backup.c_str(), 0, nullptr, nullptr) != 0;
        } else {
            replaced = MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH) != 0;
        }
        if (!replaced) throw std::system_error(GetLastError(), std::system_category(), "replace adventure save");
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}

void AdventureStore::copy(const std::size_t source, const std::size_t destination)
{
    if (source == destination) throw std::invalid_argument("cannot copy an adventure log onto itself");
    const auto save = content::AdventureSaveFile::load(path(source));
    write(destination, save);
}

void AdventureStore::erase(const std::size_t slot)
{
    const auto destination = path(slot);
    if (!std::filesystem::exists(destination)) throw std::runtime_error("adventure log is already empty");
    auto erased = destination;
    erased += ".erased";
    if (!MoveFileExW(destination.c_str(), erased.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        throw std::system_error(GetLastError(), std::system_category(), "erase adventure log");
    }
}

const std::filesystem::path& AdventureStore::directory() const noexcept
{
    return directory_;
}

} // namespace dw4::desktop