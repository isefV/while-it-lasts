#include <while_it_lasts/system/assets.hpp>

namespace while_it_lasts
{
    Assets::Assets(std::string root)
        : root_(std::move(root))
    {
        if (!root_.empty() && root_.back() != '/' && root_.back() != '\\') root_ += '/';
    }

    Assets::~Assets()
    {
        unloadAll();
    }

    std::string Assets::path(std::string_view relative) const
    {
        return root_ + std::string(relative);
    }

    bool Assets::exists(std::string_view relative) const
    {
        return !relative.empty() && FileExists(path(relative).c_str());
    }

    const Texture2D* Assets::texture(const std::string& relative)
    {
        if (relative.empty()) return nullptr;
        auto it = textures_.find(relative);
        if (it == textures_.end())
        {
            Texture2D texture{};
            if (exists(relative))
            {
                texture = LoadTexture(path(relative).c_str());
                if (IsTextureValid(texture))
                    SetTextureFilter(texture, TEXTURE_FILTER_POINT);
                else
                    texture = Texture2D{};
            }
            it = textures_.emplace(relative, texture).first;
        }
        return it->second.id != 0 ? &it->second : nullptr;
    }

    void Assets::unloadAll()
    {
        for (auto& [name, texture] : textures_)
            if (texture.id != 0) UnloadTexture(texture);
        textures_.clear();
    }
} // namespace while_it_lasts
