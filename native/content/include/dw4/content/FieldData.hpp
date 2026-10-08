#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace dw4::content {

using FieldPalette = std::array<std::array<std::uint8_t, 4>, 4>;

struct FieldTile final {
    std::array<std::uint16_t, 4> quadrants{};
    std::uint8_t palette{};
    std::uint8_t behavior{};
};

struct FieldSpriteFrame final {
    std::array<std::uint16_t, 4> quadrants{};
    std::uint8_t palette{};
    std::array<bool, 4> flip_horizontal{};
};

struct FieldSprite final {
    std::uint16_t id{};
    std::array<FieldSpriteFrame, 8> frames;
};

struct FieldActor final {
    struct Effect final {
        std::uint8_t kind{};
        std::uint16_t argument{}, value{}, extra{};
        bool invert{};
        std::vector<Effect> yes, no;
    };
    std::uint16_t id{}, sprite{}, message{};
    std::int16_t x{}, y{};
    std::uint8_t facing{}, motion{};
    std::vector<std::uint8_t> script;
    std::vector<std::array<std::int16_t, 2>> patrol;
    bool follow_player{};
    bool remove_at_end{};
    std::vector<Effect> interaction;
};

struct FieldGrowth final {
    struct Level final {
        std::uint32_t experience{};
        std::array<std::uint16_t, 7> targets{}, gains{};
    };
    std::uint8_t id{};
    std::array<Level, 99> levels;
};

struct FieldConnection final {
    std::int16_t x{}, y{};
    std::uint8_t map{}, submap{};
    std::int16_t destination_x{}, destination_y{};
};

struct FieldMap final {
    std::uint8_t id{}, submap{}, border{};
    std::uint16_t width{}, height{};
    std::vector<std::uint8_t> cells;
    std::vector<FieldTile> tiles;
    std::array<FieldPalette, 4> palettes;
    std::vector<FieldActor> actors;
    std::vector<FieldConnection> connections;
    std::optional<FieldConnection> boundary_destination;
};

struct FieldItem final {
    std::uint16_t id{}, power{};
    std::string name;
    std::uint32_t price{};
    std::uint8_t slot{}, equip_mask{};
    bool cursed{};
};

struct FieldShop final {
    std::uint8_t map{}, submap{}, mode{};
    std::vector<std::uint16_t> items;
};

struct FieldTreasure final {
    std::uint8_t map{}, submap{};
    std::int16_t x{}, y{};
    std::uint16_t flag{};
    std::optional<std::uint16_t> item;
    std::uint32_t gold{};
};

struct FieldInn final { std::uint8_t map{}, submap{}, price{}; };

struct FieldMonster final {
    struct Draw final { std::uint16_t tile{}; std::int16_t x{}, y{}; std::uint8_t palette{}, flips{}; };
    std::uint16_t id{}, hp{}, mp{}, attack{}, defense{}, agility{};
    std::string name;
    std::uint32_t experience{}, gold{};
    std::optional<std::uint16_t> drop;
    std::uint32_t drop_numerator{}, drop_denominator{1};
    std::vector<std::array<std::uint16_t, 2>> actions;
    std::uint16_t width{}, height{};
    std::vector<FieldPalette> palettes;
    std::vector<Draw> draws;
};

struct FieldEncounters final {
    struct Group final {
        std::uint8_t rate{}, control{};
        std::array<std::uint8_t, 14> entries{}, weights{};
        std::array<std::uint8_t, 4> extra{};
    };
    std::array<std::uint8_t, 256> grid{};
    std::array<Group, 64> groups;
    std::array<std::array<std::uint8_t, 6>, 256> mixed;
    std::array<std::uint8_t, 8> terrain_rates{}, mixed_span{}, mixed_base{}, single_span{}, single_base{};
    std::array<std::uint8_t, 3> first_steps{};
};

class FieldData final {
public:
    [[nodiscard]] static FieldData load(const std::filesystem::path& directory);
    [[nodiscard]] const FieldMap& map(std::uint8_t id, std::uint8_t submap) const;
    [[nodiscard]] const FieldSprite& sprite(std::uint16_t id) const;
    [[nodiscard]] std::uint16_t tile_count() const noexcept;
    [[nodiscard]] const std::filesystem::path& image() const noexcept;
    [[nodiscard]] const std::array<FieldPalette, 4>& sprite_palettes() const noexcept;
    [[nodiscard]] const std::vector<FieldMap>& maps() const noexcept;
    [[nodiscard]] const std::vector<FieldGrowth>& progression() const noexcept;
    [[nodiscard]] const std::vector<FieldItem>& items() const noexcept;
    [[nodiscard]] const std::vector<FieldShop>& shops() const noexcept;
    [[nodiscard]] const std::vector<FieldMonster>& monsters() const noexcept;
    [[nodiscard]] const FieldEncounters& encounters() const noexcept;
    [[nodiscard]] const std::vector<FieldTreasure>& treasures() const noexcept;
    [[nodiscard]] const std::vector<FieldInn>& inns() const noexcept;

private:
    std::filesystem::path image_;
    std::uint16_t tile_count_{};
    std::vector<FieldMap> maps_;
    std::vector<FieldSprite> sprites_;
    std::array<FieldPalette, 4> sprite_palettes_;
    std::vector<FieldGrowth> progression_;
    std::vector<FieldItem> items_;
    std::vector<FieldShop> shops_;
    std::vector<FieldMonster> monsters_;
    FieldEncounters encounters_;
    std::vector<FieldTreasure> treasures_;
    std::vector<FieldInn> inns_;
};

} // namespace dw4::content