#pragma once

/**
 * @file item.hpp
 * @brief Inventory items, from assets/data/items.json.
 */

#include <raylib.h>

#include <string>

namespace while_it_lasts
{
    struct ItemDef
    {
        std::string id;
        std::string name;
        std::string description;
        std::string icon;            ///< 16 x 16 image, relative to assets/; a coloured square until it exists
        Color color{120, 130, 140, 255};

        /// When set, the item is shown while this variable is above 0, with the
        /// value as its count ("Cigarettes x2"), instead of by give/take.
        std::string countVar;

        /// Conversation played when the player uses the item from the
        /// inventory (E); empty: the item cannot be used there.
        std::string use;
    };
} // namespace while_it_lasts
