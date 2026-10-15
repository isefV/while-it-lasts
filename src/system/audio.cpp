#include <hollow/system/assets.hpp>
#include <hollow/system/audio.hpp>
#include <hollow/utility/math.hpp>

#include <raylib.h>

#include <algorithm>
#include <cstdlib>
#include <map>

namespace hollow
{
    namespace
    {
        constexpr float FadeSpeed = 0.8f;   // volume per second
        constexpr float MusicVolume = 0.45f; // under the ambience and the dialogue

        struct Loop
        {
            Music music{};
            float volume = 0.0f;
            float target = 1.0f;
        };
    } // namespace

    struct Audio::Impl
    {
        Assets& assets;
        bool ready = false;
        std::map<std::string, Sound> sounds; // frameCount 0: missing
        std::map<std::string, Loop> loops;   // ambient layers and music
        std::string music;

        explicit Impl(Assets& a)
            : assets(a)
        {
        }

        std::string find(const std::string& id) const
        {
            for (const char* ext : {".ogg", ".wav", ".mp3", ".qoa"})
            {
                const std::string relative = "sounds/" + id + ext;
                if (assets.exists(relative)) return assets.path(relative);
            }
            return {};
        }

        Loop* loop(const std::string& id)
        {
            auto it = loops.find(id);
            if (it == loops.end())
            {
                Loop entry;
                if (const auto path = find(id); !path.empty())
                {
                    entry.music = LoadMusicStream(path.c_str());
                    if (IsMusicValid(entry.music))
                    {
                        SetMusicVolume(entry.music, 0.0f);
                        PlayMusicStream(entry.music);
                    }
                    else
                    {
                        entry.music = Music{};
                    }
                }
                it = loops.emplace(id, entry).first;
            }
            return &it->second;
        }
    };

    Audio::Audio(Assets& assets)
        : d_(std::make_unique<Impl>(assets))
    {
    }

    Audio::~Audio()
    {
        shutdown();
    }

    void Audio::init()
    {
        InitAudioDevice();
        d_->ready = IsAudioDeviceReady();
    }

    void Audio::shutdown()
    {
        if (!d_->ready) return;
        for (auto& [id, sound] : d_->sounds)
            if (sound.frameCount > 0) UnloadSound(sound);
        for (auto& [id, loop] : d_->loops)
            if (loop.music.frameCount > 0) UnloadMusicStream(loop.music);
        d_->sounds.clear();
        d_->loops.clear();
        CloseAudioDevice();
        d_->ready = false;
    }

    void Audio::play(const std::string& id, float volume, float pitch)
    {
        if (!d_->ready || id.empty()) return;
        auto it = d_->sounds.find(id);
        if (it == d_->sounds.end())
        {
            Sound sound{};
            if (const auto path = d_->find(id); !path.empty())
            {
                sound = LoadSound(path.c_str());
                if (!IsSoundValid(sound)) sound = Sound{};
            }
            it = d_->sounds.emplace(id, sound).first;
        }
        if (it->second.frameCount == 0) return;
        SetSoundVolume(it->second, volume);
        SetSoundPitch(it->second, pitch);
        PlaySound(it->second);
    }

    void Audio::setAmbient(const std::vector<std::string>& ids)
    {
        if (!d_->ready) return;
        for (auto& [id, loop] : d_->loops)
            if (id != d_->music) loop.target = 0.0f;
        for (const auto& entry : ids)
        {
            // "fire@0.4": that loop at 40 %.
            const auto at = entry.find('@');
            const float volume = at == std::string::npos ? 1.0f : std::strtof(entry.c_str() + at + 1, nullptr);
            d_->loop(entry.substr(0, at))->target = std::clamp(volume, 0.0f, 1.0f);
        }
    }

    void Audio::setMusic(const std::string& id)
    {
        if (!d_->ready || id == d_->music) return;
        if (!d_->music.empty()) d_->loop(d_->music)->target = 0.0f;
        d_->music = id;
        if (!id.empty()) d_->loop(id)->target = MusicVolume;
    }

    void Audio::update(float dt)
    {
        if (!d_->ready) return;
        for (auto& [id, loop] : d_->loops)
        {
            if (loop.music.frameCount == 0) continue;
            loop.volume = approach(loop.volume, loop.target, FadeSpeed * dt);
            SetMusicVolume(loop.music, loop.volume);
            UpdateMusicStream(loop.music);
        }
    }
} // namespace hollow
