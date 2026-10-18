#pragma once

/**
 * @file hollow.hpp
 * @brief The Hollow library: everything the game is built from.
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

#include <hollow/data/game_data.hpp>
#include <hollow/effects/effects.hpp>
#include <hollow/objects/actor.hpp>
#include <hollow/objects/character.hpp>
#include <hollow/objects/item.hpp>
#include <hollow/objects/room.hpp>
#include <hollow/objects/sprite.hpp>
#include <hollow/system/assets.hpp>
#include <hollow/system/audio.hpp>
#include <hollow/system/canvas.hpp>
#include <hollow/system/dialogue.hpp>
#include <hollow/system/dialogue_runner.hpp>
#include <hollow/system/input.hpp>
#include <hollow/system/script.hpp>
#include <hollow/system/story_state.hpp>
#include <hollow/ui/bark.hpp>
#include <hollow/ui/dialogue_box.hpp>
#include <hollow/ui/inventory_panel.hpp>
#include <hollow/ui/text_renderer.hpp>
#include <hollow/ui/theme.hpp>
#include <hollow/ui/widgets.hpp>
#include <hollow/utility/color.hpp>
#include <hollow/utility/diagnostics.hpp>
#include <hollow/utility/math.hpp>
#include <hollow/utility/random.hpp>
#include <hollow/utility/text.hpp>

#include <string_view>

namespace hollow
{
    /// The library's version, "1.0.6".
    std::string_view version();
} // namespace hollow
