#include <while_it_lasts/system/assets.hpp>
#include <while_it_lasts/system/input.hpp>
#include <while_it_lasts/ui/inventory_panel.hpp>
#include <while_it_lasts/ui/text_renderer.hpp>
#include <while_it_lasts/ui/widgets.hpp>

#include <algorithm>

namespace while_it_lasts
{
    namespace
    {
        // Over the scene box, in canvas pixels.
        constexpr Rectangle Panel{layout::SceneBox.x + (layout::SceneBox.width - 240) / 2, layout::SceneBox.y + 34, 240, 132};
        constexpr int Columns = 2;
        constexpr float CellW = 112;
        constexpr float CellH = 20;
    } // namespace

    void InventoryPanel::open()
    {
        open_ = true;
        selected_ = 0;
    }

    Rectangle InventoryPanel::cellRect(int index) const
    {
        const int col = index % Columns;
        const int row = index / Columns;
        return Rectangle{Panel.x + 8 + static_cast<float>(col) * CellW, Panel.y + 20 + static_cast<float>(row) * CellH,
                         CellW - 4, CellH - 2};
    }

    const ItemDef* InventoryPanel::update(const std::vector<InventoryEntry>& entries, const Input& input)
    {
        if (!open_) return nullptr;
        if (input.pressed(Action::Inventory) || input.pressed(Action::Pause))
        {
            open_ = false;
            return nullptr;
        }

        const int count = static_cast<int>(entries.size());
        if (count == 0) return nullptr;
        if (input.pressed(Action::Left)) selected_ = std::max(0, selected_ - 1);
        if (input.pressed(Action::Right)) selected_ = std::min(count - 1, selected_ + 1);
        if (input.pressed(Action::Up)) selected_ = std::max(0, selected_ - Columns);
        if (input.pressed(Action::Down)) selected_ = std::min(count - 1, selected_ + Columns);
        selected_ = std::clamp(selected_, 0, count - 1);

        const ItemDef* item = entries[static_cast<std::size_t>(selected_)].item;
        if (input.pressed(Action::Confirm) && !item->use.empty())
        {
            open_ = false;
            return item;
        }
        return nullptr;
    }

    void InventoryPanel::draw(const std::vector<InventoryEntry>& entries, const TextRenderer& text,
                              const UiConfig& ui, Assets& assets) const
    {
        if (!open_) return;

        DrawRectangleRec(layout::SceneBox, ColorAlpha(BLACK, 0.55f));
        drawPanel(assets, Panel, ui);
        text.draw("Carrying", Vector2{Panel.x + 8, Panel.y + 6}, ui.dim);
        const bool usable = !entries.empty() &&
                            !entries[static_cast<std::size_t>(std::clamp(selected_, 0, static_cast<int>(entries.size()) - 1))].item->use.empty();
        const char* keys = usable ? "E: use   Tab: close" : "Tab: close";
        text.draw(keys, Vector2{Panel.x + Panel.width - 8 - text.measure(keys), Panel.y + 6}, ui.dim);

        if (entries.empty())
            text.draw("Nothing but lint.", Vector2{Panel.x + 8, Panel.y + 24}, ui.narration);

        for (int i = 0; i < static_cast<int>(entries.size()); ++i)
        {
            const InventoryEntry& entry = entries[static_cast<std::size_t>(i)];
            const Rectangle cell = cellRect(i);
            const bool active = i == selected_;
            if (active) DrawRectangleRec(cell, ColorAlpha(ui.frame, 0.45f));

            const Rectangle icon{cell.x + 1, cell.y + 1, 16, 16};
            if (const Texture2D* texture = assets.texture(entry.item->icon))
                DrawTexture(*texture, static_cast<int>(icon.x), static_cast<int>(icon.y), WHITE);
            else
                DrawRectangleRec(Rectangle{icon.x + 2, icon.y + 2, 12, 12}, entry.item->color);

            std::string label = entry.item->name;
            if (entry.count > 1) label += " x" + std::to_string(entry.count);
            text.draw(label, Vector2{cell.x + 21, cell.y + 5}, active ? ui.highlight : ui.text);
            if (!entry.item->use.empty()) // a small mark: this one can be used
                DrawRectangleRec(Rectangle{cell.x + cell.width - 4, cell.y + 7, 2, 2}, ColorAlpha(ui.thought, active ? 1.0f : 0.5f));
        }

        // The description, in the dialogue box.
        drawPanel(assets, layout::DialogueBox, ui);
        if (!entries.empty())
        {
            const auto& item = *entries[static_cast<std::size_t>(std::clamp(selected_, 0, static_cast<int>(entries.size()) - 1))].item;
            const auto lines = wrapText(item.description, layout::DialogueBox.width - 2 * layout::Padding, text.measureFn());
            float y = layout::DialogueBox.y + layout::Padding;
            for (std::size_t i = 0; i < lines.size() && i < 4; ++i, y += layout::LineHeight)
                text.draw(lines[i], Vector2{layout::DialogueBox.x + layout::Padding, y}, ui.text);
        }
    }
} // namespace while_it_lasts
