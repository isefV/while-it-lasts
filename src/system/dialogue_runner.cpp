#include <while_it_lasts/system/dialogue_runner.hpp>
#include <while_it_lasts/system/story_state.hpp>

namespace while_it_lasts
{
    namespace
    {
        struct Frame
        {
            const std::vector<DialogueEntry>* entries = nullptr;
            std::size_t index = 0;
        };
    } // namespace

    struct DialogueRunner::Impl
    {
        const ConversationMap& conversations;
        StoryState& state;
        CommandHandler onCommand;
        TokenMap tokens;

        std::vector<Frame> stack;
        DialogueView view;
        const DialogueEntry* waitingOn = nullptr;  // the line, choice or pause on screen
        std::vector<const DialogueOption*> shown;  // options on screen, in order
        std::string currentId;
        unsigned generation = 0;                   // changes on start()/stop()

        Impl(const ConversationMap& c, StoryState& s, CommandHandler h)
            : conversations(c)
            , state(s)
            , onCommand(std::move(h))
        {
        }

        // Applies effects; returns false if an effect's command started or
        // stopped a conversation (the caller must not touch the old one).
        bool apply(const std::vector<Effect>& effects)
        {
            const unsigned before = generation;
            applyEffects(effects, state, [this](const Effect& e) {
                if (onCommand) onCommand(e);
            });
            return generation == before;
        }

        bool push(const std::string& id)
        {
            const auto it = conversations.find(id);
            if (it == conversations.end()) return false;
            state.markSeen(id);
            currentId = id;
            stack.push_back(Frame{&it->second.entries, 0});
            return true;
        }

        void clearView()
        {
            const unsigned serial = view.serial;
            view = DialogueView{};
            view.serial = serial;
            waitingOn = nullptr;
            shown.clear();
        }

        void show(DialogueView::Kind kind, const DialogueEntry& entry)
        {
            const unsigned serial = view.serial + 1;
            view = DialogueView{};
            view.serial = serial;
            view.kind = kind;
            waitingOn = &entry;
        }

        // Moves on until something has to be shown or the conversation ends.
        void step()
        {
            clearView();
            while (!stack.empty())
            {
                Frame& frame = stack.back();
                if (frame.index >= frame.entries->size())
                {
                    stack.pop_back();
                    continue;
                }
                const DialogueEntry& entry = (*frame.entries)[frame.index++];

                if (!entry.condition.evaluate(state))
                {
                    if (entry.kind == DialogueEntry::Kind::Block && !entry.elseEntries.empty())
                        stack.push_back(Frame{&entry.elseEntries, 0});
                    continue;
                }

                switch (entry.kind)
                {
                case DialogueEntry::Kind::Line:
                    show(DialogueView::Kind::Line, entry);
                    view.style = entry.style;
                    view.speaker = entry.speaker;
                    view.text = replaceTokens(entry.text, tokens);
                    apply(entry.effects);
                    return;

                case DialogueEntry::Kind::Choice:
                {
                    std::vector<const DialogueOption*> visible;
                    for (const auto& option : entry.options)
                        if (!option.text.empty() && option.condition.evaluate(state)) visible.push_back(&option);
                    if (!apply(entry.effects)) return;
                    if (visible.empty()) continue;

                    show(DialogueView::Kind::Choice, entry);
                    shown = std::move(visible);
                    for (const auto* option : shown)
                        view.options.push_back(replaceTokens(option->text, tokens));
                    view.timer = entry.timer;
                    view.timeLeft = entry.timer;
                    return;
                }

                case DialogueEntry::Kind::Goto:
                    if (!apply(entry.effects)) return;
                    stack.clear();
                    push(entry.target);
                    continue;

                case DialogueEntry::Kind::Pause:
                    show(DialogueView::Kind::Pause, entry);
                    view.timeLeft = entry.seconds;
                    apply(entry.effects);
                    return;

                case DialogueEntry::Kind::Block:
                    if (!apply(entry.effects)) return;
                    stack.push_back(Frame{&entry.thenEntries, 0});
                    continue;

                case DialogueEntry::Kind::Action:
                    if (!apply(entry.effects)) return;
                    continue;

                case DialogueEntry::Kind::End:
                    apply(entry.effects);
                    stack.clear();
                    return;
                }
            }
        }

        void pick(const DialogueOption& option)
        {
            const unsigned before = generation;
            waitingOn = nullptr;
            if (!apply(option.effects)) return;
            if (!option.next.empty())
            {
                stack.clear();
                push(option.next);
            }
            if (generation == before) step();
        }
    };

    DialogueRunner::DialogueRunner(const ConversationMap& conversations, StoryState& state, CommandHandler onCommand)
        : d_(std::make_unique<Impl>(conversations, state, std::move(onCommand)))
    {
    }

    DialogueRunner::~DialogueRunner() = default;

    void DialogueRunner::setTokens(TokenMap tokens)
    {
        d_->tokens = std::move(tokens);
    }

    bool DialogueRunner::start(const std::string& id)
    {
        ++d_->generation;
        d_->stack.clear();
        if (!d_->push(id))
        {
            d_->clearView();
            return false;
        }
        d_->step();
        return active();
    }

    void DialogueRunner::stop()
    {
        ++d_->generation;
        d_->stack.clear();
        d_->clearView();
    }

    void DialogueRunner::update(float dt)
    {
        DialogueView& view = d_->view;
        if (view.kind == DialogueView::Kind::Pause)
        {
            view.timeLeft -= dt;
            if (view.timeLeft <= 0.0f) d_->step();
            return;
        }
        if (view.kind == DialogueView::Kind::Choice && view.timer > 0.0f)
        {
            view.timeLeft -= dt;
            if (view.timeLeft > 0.0f) return;

            const DialogueEntry* entry = d_->waitingOn;
            for (const auto& option : entry->options)
            {
                if (option.timeout && option.condition.evaluate(d_->state))
                {
                    d_->pick(option);
                    return;
                }
            }
            d_->step(); // no timeout option: go on below the choice
        }
    }

    void DialogueRunner::advance()
    {
        if (d_->view.kind == DialogueView::Kind::Line) d_->step();
    }

    void DialogueRunner::choose(int index)
    {
        if (d_->view.kind != DialogueView::Kind::Choice) return;
        if (index < 0 || index >= static_cast<int>(d_->shown.size())) return;
        d_->pick(*d_->shown[static_cast<std::size_t>(index)]);
    }

    bool DialogueRunner::active() const
    {
        return d_->view.kind != DialogueView::Kind::None;
    }

    const DialogueView& DialogueRunner::view() const
    {
        return d_->view;
    }

    const std::string& DialogueRunner::currentId() const
    {
        return d_->currentId;
    }
} // namespace while_it_lasts
