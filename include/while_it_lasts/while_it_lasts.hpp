#pragma once

/**
 * @file while_it_lasts.hpp
 * @brief The While It Lasts library: everything the game is built from.
 *
 * Headers declare; the code lives in src/. Systems with a lot of private
 * state (StoryState, DialogueRunner, Audio) keep it behind a d-pointer
 * (`d_`); small value types keep plain members.
 *
 *   utility/   text, random picks, colours, diagnostics
 *   system/    canvas, input, assets, audio, story state, conditions and
 *              effects, dialogue and the runner that plays it
 *   data/      loading and checking the files in assets/data/
 *   objects/   items, characters, rooms, sprite sheets, actors
 *   effects/   fades, shake, firelight, apparitions, particles
 *   ui/        layout and colours, text, dialogue box, inventory, widgets, barks
 *
 * The game itself (rooms' logic, the main loop) is in apps/app.
 */

#include <while_it_lasts/data/game_data.hpp>
#include <while_it_lasts/effects/effects.hpp>
#include <while_it_lasts/objects/actor.hpp>
#include <while_it_lasts/objects/character.hpp>
#include <while_it_lasts/objects/item.hpp>
#include <while_it_lasts/objects/room.hpp>
#include <while_it_lasts/objects/sprite.hpp>
#include <while_it_lasts/system/assets.hpp>
#include <while_it_lasts/system/audio.hpp>
#include <while_it_lasts/system/canvas.hpp>
#include <while_it_lasts/system/dialogue.hpp>
#include <while_it_lasts/system/dialogue_runner.hpp>
#include <while_it_lasts/system/input.hpp>
#include <while_it_lasts/system/script.hpp>
#include <while_it_lasts/system/story_state.hpp>
#include <while_it_lasts/ui/bark.hpp>
#include <while_it_lasts/ui/dialogue_box.hpp>
#include <while_it_lasts/ui/inventory_panel.hpp>
#include <while_it_lasts/ui/text_renderer.hpp>
#include <while_it_lasts/ui/theme.hpp>
#include <while_it_lasts/ui/widgets.hpp>
#include <while_it_lasts/utility/color.hpp>
#include <while_it_lasts/utility/diagnostics.hpp>
#include <while_it_lasts/utility/math.hpp>
#include <while_it_lasts/utility/random.hpp>
#include <while_it_lasts/utility/text.hpp>

#include <string_view>

namespace while_it_lasts
{
    /// The library's version, "1.2.0".
    std::string_view version();
} // namespace while_it_lasts
