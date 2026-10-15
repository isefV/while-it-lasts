#pragma once

/**
 * @file inventory_panel.hpp
 * @brief The inventory: opened with Tab or I, it lists what the player
 * carries, and the dialogue box shows the selected item's description.
 * Items are used through options at the thing they are used on.
 */

#include <hollow/objects/item.hpp>
#include <hollow/ui/theme.hpp>

#include <vector>

namespace hollow
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
        [[nodiscard]] bool isOpen() const { return open_; }

        /// Arrow keys (or WASD) while open; closes on Tab, I or Esc.
        void update(const std::vector<InventoryEntry>& entries, const Input& input);
        void draw(const std::vector<InventoryEntry>& entries, const TextRenderer& text, const UiConfig& ui,
                  Assets& assets) const;

    private:
        [[nodiscard]] Rectangle cellRect(int index) const;

        bool open_ = false;
        int selected_ = 0;
    };
} // namespace hollow
