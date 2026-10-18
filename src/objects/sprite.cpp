#include <hollow/objects/sprite.hpp>
#include <hollow/system/assets.hpp>

#include "../utility/json_util.hpp"

#include <cmath>

namespace hollow
{
    using detail::child;
    using detail::Json;

    std::optional<SpriteSheet> loadSpriteSheet(Assets& assets, const std::string& config, Diagnostics& diag)
    {
        if (!assets.exists(config)) return std::nullopt;
        const auto root = detail::loadJsonFile(assets.path(config), config, diag);
        if (!root || !root->is_object()) return std::nullopt;

        SpriteSheet sheet;
        const std::string where = config;
        detail::checkKeys(*root, {"texture", "frame", "origin", "animations"}, where, diag);
        sheet.texture = assets.texture(detail::readString(*root, "texture", where, diag));

        if (root->contains("frame"))
        {
            const Json& frame = (*root)["frame"];
            sheet.frameWidth = detail::readInt(frame, "w", child(where, "frame"), diag, sheet.frameWidth);
            sheet.frameHeight = detail::readInt(frame, "h", child(where, "frame"), diag, sheet.frameHeight);
        }
        sheet.origin = Vector2{static_cast<float>(sheet.frameWidth) / 2.0f, static_cast<float>(sheet.frameHeight)};
        if (root->contains("origin"))
        {
            const Json& origin = (*root)["origin"];
            sheet.origin.x = detail::readNumber(origin, "x", child(where, "origin"), diag, sheet.origin.x);
            sheet.origin.y = detail::readNumber(origin, "y", child(where, "origin"), diag, sheet.origin.y);
        }
        if (root->contains("animations") && (*root)["animations"].is_object())
        {
            for (const auto& [name, obj] : (*root)["animations"].items())
            {
                const std::string at = child(child(where, "animations"), name);
                detail::checkKeys(obj, {"row", "column", "frames", "fps", "loop", "frame", "origin", "top"}, at, diag);
                Animation animation;
                animation.row = detail::readInt(obj, "row", at, diag);
                animation.column = detail::readInt(obj, "column", at, diag, 0);
                animation.frames = detail::readInt(obj, "frames", at, diag, 1);
                animation.fps = detail::readNumber(obj, "fps", at, diag, 1.0f);
                animation.loop = detail::readBool(obj, "loop", at, diag, true);

                animation.frameWidth = sheet.frameWidth;
                animation.frameHeight = sheet.frameHeight;
                if (obj.contains("frame"))
                {
                    const Json& frame = obj["frame"];
                    animation.frameWidth = detail::readInt(frame, "w", child(at, "frame"), diag, sheet.frameWidth);
                    animation.frameHeight = detail::readInt(frame, "h", child(at, "frame"), diag, sheet.frameHeight);
                }
                // A row of other-sized frames keeps its feet at the bottom middle unless told otherwise.
                animation.origin = obj.contains("frame") ? Vector2{static_cast<float>(animation.frameWidth) / 2.0f,
                                                                   static_cast<float>(animation.frameHeight)}
                                                         : sheet.origin;
                if (obj.contains("origin"))
                {
                    const Json& origin = obj["origin"];
                    animation.origin.x = detail::readNumber(origin, "x", child(at, "origin"), diag, animation.origin.x);
                    animation.origin.y = detail::readNumber(origin, "y", child(at, "origin"), diag, animation.origin.y);
                }
                animation.top = detail::readInt(obj, "top", at, diag, animation.row * sheet.frameHeight);
                sheet.animations[name] = animation;
            }
        }
        return sheet;
    }

    void Animator::play(const std::string& name)
    {
        if (name == current_) return;
        current_ = name;
        time_ = 0.0f;
        frame_ = 0;
    }

    void Animator::update(float dt, const SpriteSheet* sheet)
    {
        time_ += dt;
        if (!sheet) return;
        const auto it = sheet->animations.find(current_);
        if (it == sheet->animations.end() || it->second.frames <= 1)
        {
            frame_ = 0;
            return;
        }
        const Animation& animation = it->second;
        const int step = static_cast<int>(std::floor(time_ * animation.fps));
        frame_ = animation.loop ? step % animation.frames : std::min(step, animation.frames - 1);
    }

    namespace
    {
        const Animation* currentAnimation(const SpriteSheet& sheet, const Animator& animator)
        {
            if (!sheet.texture) return nullptr;
            const auto it = sheet.animations.find(animator.current());
            return it == sheet.animations.end() ? nullptr : &it->second;
        }

        Rectangle frameSource(const Animation& animation, const Animator& animator, bool flipX)
        {
            const auto w = static_cast<float>(animation.frameWidth);
            const auto h = static_cast<float>(animation.frameHeight);
            return Rectangle{static_cast<float>(animation.column + animator.frame()) * w, static_cast<float>(animation.top),
                             flipX ? -w : w, h};
        }
    } // namespace

    bool drawSprite(const SpriteSheet& sheet, const Animator& animator, Vector2 feet, bool flipX, Color tint, float scale)
    {
        const Animation* animation = currentAnimation(sheet, animator);
        if (!animation) return false;
        const auto w = static_cast<float>(animation->frameWidth);
        const auto h = static_cast<float>(animation->frameHeight);
        const float originX = flipX ? w - animation->origin.x : animation->origin.x;
        const Rectangle dest{std::round(feet.x - originX * scale), std::round(feet.y - animation->origin.y * scale),
                             std::round(w * scale), std::round(h * scale)};
        DrawTexturePro(*sheet.texture, frameSource(*animation, animator, flipX), dest, Vector2{0, 0}, 0.0f, tint);
        return true;
    }

    bool drawSpriteIn(const SpriteSheet& sheet, const Animator& animator, Rectangle dest, bool flipX, Color tint)
    {
        const Animation* animation = currentAnimation(sheet, animator);
        if (!animation) return false;
        DrawTexturePro(*sheet.texture, frameSource(*animation, animator, flipX), dest, Vector2{0, 0}, 0.0f, tint);
        return true;
    }
} // namespace hollow
