#pragma once

/**
 * @file audio.hpp
 * @brief Sound effects, ambient loops and music from assets/sounds/.
 *
 * A sound id "crack" plays assets/sounds/crack.ogg (or .wav, .mp3, .qoa).
 * Files that do not exist yet are skipped without a word, so the game runs
 * before any sound is made. Credits and licences: assets/sounds/CREDITS.md.
 */

#include <memory>
#include <string>
#include <vector>

namespace hollow
{
    class Assets;

    class Audio
    {
    public:
        explicit Audio(Assets& assets);
        ~Audio();

        Audio(const Audio&) = delete;
        Audio& operator=(const Audio&) = delete;

        /// Opens the audio device; without it every call does nothing.
        void init();
        void shutdown();

        void play(const std::string& id, float volume = 1.0f);

        /// The loops that should be playing; others fade out, new ones fade in.
        void setAmbient(const std::vector<std::string>& ids);

        /// Music track; empty stops it.
        void setMusic(const std::string& id);

        /// Feeds the streams and runs the fades; call every frame.
        void update(float dt);

    private:
        struct Impl;
        std::unique_ptr<Impl> d_;
    };
} // namespace hollow
