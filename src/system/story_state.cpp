#include <while_it_lasts/system/story_state.hpp>

#include <algorithm>
#include <map>

namespace while_it_lasts
{
    namespace
    {
        struct Variable
        {
            int value = 0;
            int start = 0;
            int min = 0;
            int max = 0;
        };

        constexpr std::string_view ItemPrefix = "item.";
        constexpr std::string_view SeenPrefix = "seen.";
    } // namespace

    struct StoryState::Impl
    {
        std::map<std::string, Variable, std::less<>> variables;
        std::vector<std::string> items;
        std::map<std::string, int, std::less<>> seen;
    };

    StoryState::StoryState()
        : d_(std::make_unique<Impl>())
    {
    }

    StoryState::~StoryState() = default;
    StoryState::StoryState(StoryState&&) noexcept = default;
    StoryState& StoryState::operator=(StoryState&&) noexcept = default;

    void StoryState::define(const std::string& name, int value, int min, int max)
    {
        if (max < min) std::swap(min, max);
        value = std::clamp(value, min, max);
        d_->variables[name] = Variable{value, value, min, max};
    }

    bool StoryState::defined(std::string_view name) const
    {
        return d_->variables.find(name) != d_->variables.end();
    }

    int StoryState::get(std::string_view name) const
    {
        if (name.starts_with(ItemPrefix)) return hasItem(name.substr(ItemPrefix.size())) ? 1 : 0;
        if (name.starts_with(SeenPrefix)) return seen(name.substr(SeenPrefix.size()));

        const auto it = d_->variables.find(name);
        return it == d_->variables.end() ? 0 : it->second.value;
    }

    void StoryState::set(std::string_view name, int value)
    {
        const auto it = d_->variables.find(name);
        if (it == d_->variables.end()) return;
        it->second.value = std::clamp(value, it->second.min, it->second.max);
    }

    void StoryState::add(std::string_view name, int delta)
    {
        set(name, get(name) + delta);
    }

    bool StoryState::hasItem(std::string_view id) const
    {
        return std::find(d_->items.begin(), d_->items.end(), id) != d_->items.end();
    }

    void StoryState::give(const std::string& id)
    {
        if (!hasItem(id)) d_->items.push_back(id);
    }

    void StoryState::take(std::string_view id)
    {
        const auto it = std::find(d_->items.begin(), d_->items.end(), id);
        if (it != d_->items.end()) d_->items.erase(it);
    }

    const std::vector<std::string>& StoryState::items() const
    {
        return d_->items;
    }

    void StoryState::markSeen(const std::string& conversation)
    {
        ++d_->seen[conversation];
    }

    int StoryState::seen(std::string_view conversation) const
    {
        const auto it = d_->seen.find(conversation);
        return it == d_->seen.end() ? 0 : it->second;
    }

    void StoryState::reset()
    {
        for (auto& [name, variable] : d_->variables)
            variable.value = variable.start;
        d_->items.clear();
        d_->seen.clear();
    }

    StoryState StoryState::snapshot() const
    {
        StoryState copy;
        *copy.d_ = *d_;
        return copy;
    }

    void StoryState::restore(const StoryState& from)
    {
        *d_ = *from.d_;
    }
} // namespace while_it_lasts
