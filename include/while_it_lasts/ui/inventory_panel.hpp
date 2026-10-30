#pragma once

/**
 * @file inventory_panel.hpp
 * @brief The inventory: opened with Tab or I, it lists what the player
 * carries, and the dialogue box shows the selected item's description.
 * E uses the selected item when it has a "use" conversation (a bandage, the
 * antibiotics); other items are used through options at the thing they are
 * used on.
 */

#include <while_it_lasts/objects/item.hpp>
#include <while_it_lasts/ui/theme.hpp>

#include <vector>

namespace while_it_lasts
{
    class Assets;
    class Input;
    class TextRenderer;

    struct InventoryEntry
    {
        const ItemDef* item = nullptr;
        int count = 0; ///< 0: shown without a number
    };

    class InventoryPanel
    {
    public:
        void open();
        void close() { open_ = false; }
        void select(int index) { selected_ = index; }
        [[nodiscard]] bool isOpen() const { return open_; }

        /// Arrow keys (or WASD) while open; closes on Tab, I or Esc. Returns
        /// the item to use when E or Enter was pressed on one that can be used
        /// (the panel then closes), else nullptr.
        const ItemDef* update(const std::vector<InventoryEntry>& entries, const Input& input);
        void draw(const std::vector<InventoryEntry>& entries, const TextRenderer& text, const UiConfig& ui,
                  Assets& assets) const;

    private:
        [[nodiscard]] Rectangle cellRect(int index) const;

        bool open_ = false;
        int selected_ = 0;
    };
} // namespace while_it_lasts
