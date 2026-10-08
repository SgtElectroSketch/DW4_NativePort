#pragma once

#include <dw4/content/AdventureSave.hpp>

#include <array>
#include <filesystem>
#include <optional>
#include <string>

namespace dw4::desktop {

struct AdventureSlot final {
    std::optional<content::AdventureSave> save;
    std::string error;
};

class AdventureStore final {
public:
    explicit AdventureStore(std::filesystem::path directory);
    [[nodiscard]] std::array<AdventureSlot, 3> inspect() const;
    void write(std::size_t slot, const content::AdventureSave& save);
    void copy(std::size_t source, std::size_t destination);
    void erase(std::size_t slot);
    [[nodiscard]] const std::filesystem::path& directory() const noexcept;

private:
    [[nodiscard]] std::filesystem::path path(std::size_t slot) const;
    std::filesystem::path directory_;
};

} // namespace dw4::desktop