#pragma once

/**
 * @file assets.hpp
 * @brief Files in assets/: images loaded once and kept.
 *
 * A missing image is not an error: the game draws a coloured rectangle in
 * its place, so art never blocks a playable build.
 */

#include <raylib.h>

#include <map>
#include <string>
#include <string_view>

namespace while_it_lasts
{
    class Assets
    {
    public:
        explicit Assets(std::string root);
        ~Assets();

        Assets(const Assets&) = delete;
        Assets& operator=(const Assets&) = delete;

        /// The assets folder, ending with '/'.
        [[nodiscard]] const std::string& root() const { return root_; }

        /// Full path of a file in assets/.
        [[nodiscard]] std::string path(std::string_view relative) const;

        [[nodiscard]] bool exists(std::string_view relative) const;

        /// The image at @p relative, or nullptr while it does not exist.
        /// Needs the window; each file is loaded once.
        const Texture2D* texture(const std::string& relative);

        void unloadAll();

    private:
        std::string root_;
        std::map<std::string, Texture2D, std::less<>> textures_; // id 0: missing
    };
} // namespace while_it_lasts
