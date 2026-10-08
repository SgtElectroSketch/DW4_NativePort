#pragma once

#include "FieldView.hpp"
#include "AdventureStore.hpp"

#include <dw4/game/Interaction.hpp>
#include <dw4/game/AdventureLog.hpp>
#include <dw4/game/Inventory.hpp>
#include <dw4/game/Progression.hpp>
#include <dw4/game/MenuInput.hpp>
#include <dw4/game/Battle.hpp>
#include <dw4/game/Encounters.hpp>

#include <memory>
#include <optional>
#include <string>

namespace dw4::desktop {

class GameplaySession final {
public:
    GameplaySession(const content::AssetCatalog& catalog, const content::FieldData& data, std::uint32_t atlas,
                    const content::AdventureSave& save, std::uint8_t slot, AdventureStore& store);
    void tick(game::Game& game);
    void draw(GraphicsRenderer& renderer) const;
    void save(const game::FrameState& frame);
    [[nodiscard]] bool leaving() const noexcept;
    [[nodiscard]] const game::FieldState& state() const noexcept;
    [[nodiscard]] bool dialogue_active() const noexcept;
    [[nodiscard]] bool battle_active() const noexcept;
    [[nodiscard]] std::size_t validate_actor_dialogues() const;
    [[nodiscard]] static content::AdventureSave create_save(const game::LogRequest& request, const game::FrameState& frame);

private:
    enum class Page : std::uint8_t { field, commands, status, inventory, item_action, question, trade, merchant, selling, church, inn, battle, target, leaving };
    void show_message(std::uint16_t id);
    void show_text(std::string_view text);
    void advance_interaction(std::optional<bool> response = std::nullopt);
    void begin_opening();
    void advance_opening();
    void open_shop(std::uint16_t mode);
    void open_interaction(std::uint16_t actor);
    void try_encounter(game::Game& game);
    void battle_turn(game::BattleCommand command, std::size_t target, game::Game& game);
    void finish_battle(game::Game& game);
    [[nodiscard]] game::DialogueContext dialogue_context() const;
    [[nodiscard]] std::vector<std::uint16_t> text_glyphs(std::string_view text) const;
    [[nodiscard]] const content::FieldItem& item(std::uint16_t id) const;

    const content::AssetCatalog& catalog_;
    const content::FieldData& data_;
    AdventureStore& store_;
    std::uint8_t slot_{};
    FieldView view_;
    game::Field field_;
    game::MenuInput input_;
    std::vector<game::ItemRule> items_;
    std::array<game::LevelCurve, 9> curves_;
    game::EncounterRules encounters_;
    std::unique_ptr<game::Battle> battle_;
    std::unique_ptr<game::Dialogue> dialogue_;
    std::unique_ptr<game::Interaction> interaction_;
    Page page_{Page::field};
    std::uint16_t cursor_{}, selected_item_{}, shop_mode_{};
    std::vector<std::uint16_t> stock_;
    std::uint32_t inn_price_{};
    std::uint32_t opening_step_{};
    std::uint64_t last_save_{};
    bool fresh_{};
    std::uint8_t encounter_steps_{};
};

} // namespace dw4::desktop