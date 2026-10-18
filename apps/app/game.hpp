#pragma once

// The game: screens (title, playing, pause, end card), the current room, the
// player and X, and everything the room scripts in scripts/ can ask for.

#include "fight.hpp"
#include "reaper.hpp"
#include "revive.hpp"

#include <hollow/hollow.hpp>

#include <array>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

class RoomScript;

enum class CompanionMode
{
    Absent,
    Stay,
    Follow,
    Drag,
};

class Game
{
public:
    explicit Game(std::string assetsDir);
    ~Game();

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    /// Reads and checks assets/data/. Works without a window (--check).
    bool load(hollow::Diagnostics& diag);

    /// Window-dependent parts: canvas, font, sprites, sound.
    void initGraphics();
    void shutdownGraphics();

    /// One frame: input, update, draw.
    void frame(float dt);

    [[nodiscard]] bool quitRequested() const { return quit_; }

    /// For testing: a new game that starts in @p room with some variables set
    /// ("--room", "--set" on the command line). Empty @p room: the start room.
    bool startAt(const std::string& room, const std::vector<std::pair<std::string, int>>& values);

    /// Saves the 480 x 270 canvas as a PNG ("--screenshot").
    bool saveCanvas(const std::string& path);

    /// The command verbs dialogue may use in "do".
    static const std::set<std::string, std::less<>>& commands();

    /// Conversations the room scripts start by name (checked by --check).
    static const std::vector<std::string>& scriptConversations();

    // ------------------------------------------------------------------
    // For the room scripts
    // ------------------------------------------------------------------

    hollow::StoryState& state() { return state_; }
    [[nodiscard]] const hollow::GameData& data() const { return data_; }
    [[nodiscard]] const hollow::RoomDef& room() const { return *room_; }
    hollow::Assets& assets() { return assets_; }
    [[nodiscard]] const hollow::Input& input() const { return input_; }
    [[nodiscard]] const hollow::TextRenderer& text() const { return text_; }
    [[nodiscard]] RoomScript* script() { return script_.get(); }
    [[nodiscard]] Color tint() const { return tint_; }
    [[nodiscard]] const Fight& fight() const { return fight_; }
    hollow::Random& random() { return random_; }
    hollow::Particles& particles() { return particles_; }
    [[nodiscard]] const hollow::UiConfig& ui() const { return data_.ui; }
    /// Switches to the font of that name in game.json "ui" "fonts" (--font).
    bool setFont(const std::string& name);

    /// sprites/reaper.json, or nullptr while it does not exist.
    [[nodiscard]] const hollow::SpriteSheet* reaperSheet() const;

    hollow::Actor& player() { return player_; }
    hollow::Actor& companion() { return companion_; }
    [[nodiscard]] CompanionMode companionMode() const { return companionMode_; }
    void setCompanionMode(CompanionMode mode, float x = -1.0f);

    /// A conversation, a room change or the inventory has the screen.
    [[nodiscard]] bool busy() const;
    [[nodiscard]] bool dialogueActive() const;

    bool startDialogue(const std::string& id);
    void bark(const std::string& pool);
    void sound(const std::string& id, float volume = 1.0f, float pitch = 1.0f) { audio_.play(id, volume, pitch); }
    void setAmbient(const std::vector<std::string>& ids) { audio_.setAmbient(ids); }
    void shake(float seconds, float strength) { shake_.start(seconds, strength); }
    void setTint(Color tint, float seconds);
    void goTo(const std::string& room, float x = -1.0f);
    void endGame();

    /// The hiding spot the player stands in, or nullptr.
    [[nodiscard]] const hollow::ObjectDef* coverAt(float x) const;
    [[nodiscard]] bool present(const hollow::ObjectDef& object) const;
    [[nodiscard]] float timeInRoom() const { return timeInRoom_; }

private:
    friend class Walkthrough; // drives the game with virtual input

    enum class Screen
    {
        Title,
        Playing,
        Paused,
        Settings, // debug builds only, from the title or the pause menu
        Dead,     // Y_HEALTH or X_CONDITION reached 0: E tries the level again
        EndCard,
    };

    void newGame(const std::string& room = {});
    void enterRoom(const std::string& id, float spawnX);

    // Dying and trying again: everything as it was when the level began.
    void saveCheckpoint(const std::string& room, float spawnX);
    void retry();
    void checkDeath();
    void updateDead(float dt);
    [[nodiscard]] bool dead() const { return screen_ == Screen::Dead; }
    void drawDead();
    // Low health: a shake, the alarm and a note, when it drops that low.
    void updateWarnings(float dt);
    void drawWarning();
    void runEnterRules();
    void onCommand(const hollow::Effect& effect);
    hollow::Actor* actor(const std::string& id);

    void update(float dt);
    void toTitle();
    void openSettings();
    void updateSettings();
    void useFont(std::size_t index);
    void updatePlaying(float dt);
    void updateExploring(float dt);
    void updateCompanion(float dt);
    void updateTriggers();
    void interact(const hollow::ObjectDef* object);
    void talkToCompanion();
    void grabOrRelease();
    [[nodiscard]] bool canGrab() const;
    [[nodiscard]] std::vector<hollow::InventoryEntry> inventory() const;

    // What the player can use: the things in reach. E uses the nearest one,
    // or the one picked with W/S when several are in reach.
    struct Target
    {
        const hollow::ObjectDef* object = nullptr; // nullptr + companion = X
        bool companion = false;
        [[nodiscard]] bool valid() const { return object || companion; }
        bool operator==(const Target&) const = default;
    };
    [[nodiscard]] bool usable(const hollow::ObjectDef& object) const;
    [[nodiscard]] bool companionUsable() const;
    [[nodiscard]] Rectangle targetRect(const Target& target) const;
    [[nodiscard]] bool inReach(Rectangle rect) const;
    [[nodiscard]] std::vector<Target> targetsInReach() const; ///< nearest first
    [[nodiscard]] Target selectedTarget() const;

    void draw();
    void drawScene();
    void drawObject(const hollow::ObjectDef& object, bool highlighted);
    void drawEffect(const hollow::ObjectDef& object);
    void drawFrame();
    void drawSceneLabels(); ///< barks and the "[E] thing" prompt, over the scene
    [[nodiscard]] Vector2 toCanvas(Vector2 scenePoint) const;
    [[nodiscard]] const hollow::SpriteSheet* objectSheet(const hollow::ObjectDef& object) const;
    void drawHud();
    void drawTitle();
    void drawPause();
    void drawSettings();
    void drawEndCard();

    std::string objectKey(const hollow::ObjectDef& object) const;

    hollow::Assets assets_;
    hollow::Audio audio_;
    hollow::GameData data_;
    hollow::StoryState state_;
    std::unique_ptr<hollow::DialogueRunner> runner_;
    hollow::Random random_;

    hollow::Canvas canvas_;
    // Entering a room still pulling X: Y walks him in before anything starts.
    struct HaulIn
    {
        bool active = false;
        bool release = false;  // lets go of him at the end (X waits in this room)
        float returnX = -1.0f; // then steps back to here
        std::string pose;      // how X is left: "sit"
    };
    void startHaulIn();
    void updateHaulIn();
    HaulIn haul_;

    hollow::RenderLayer scene_;
    bool sceneShown_ = false; // this frame: the scene is under the canvas,
    Rectangle sceneAt_{};     // here (canvas pixels, shaken)
    hollow::Input input_;
    hollow::TextRenderer text_;
    hollow::DialogueBox dialogueBox_;
    hollow::InventoryPanel inventoryPanel_;
    hollow::BarkPlayer barks_;
    std::map<std::string, hollow::NoRepeatPicker, std::less<>> barkPickers_;
    hollow::Menu titleMenu_;
    hollow::Menu pauseMenu_;
    hollow::Menu settingsMenu_;
    Screen settingsFrom_ = Screen::Title; // where Back returns to
    std::size_t fontIndex_ = 0;           // in ui().fonts
    std::map<std::string, hollow::SpriteSheet, std::less<>> sheets_; // characters by id, and "reaper"
    std::map<std::string, hollow::SpriteSheet, std::less<>> objectSheets_; // sheet config path -> sheet

    Screen screen_ = Screen::Title;
    bool quit_ = false;
    bool graphics_ = false;

    const hollow::RoomDef* room_ = nullptr;
    std::unique_ptr<RoomScript> script_;
    hollow::Actor player_;
    hollow::Actor companion_;
    CompanionMode companionMode_ = CompanionMode::Absent;
    int dragSide_ = -1;

    // Per-run memory of rooms and objects.
    std::set<std::string> usedOnce_;
    std::map<std::string, int> useCount_;
    std::set<std::string> firedTriggers_;
    std::set<std::string> insideTriggers_;
    std::set<std::string> enterRulesDone_;

    // Room change and the end.
    hollow::Fade fade_;
    std::string pendingRoom_;
    float pendingX_ = -1.0f;
    bool ending_ = false;
    float endCardTime_ = 0.0f;

    Fight fight_;
    Revive revive_;
    Reaper reaper_;

    struct Checkpoint
    {
        bool valid = false;
        std::string room;
        float spawnX = -1.0f;
        CompanionMode mode = CompanionMode::Absent; // entering still pulling X, or not
        hollow::StoryState state;
        std::set<std::string> usedOnce;
        std::map<std::string, int> useCount;
        std::set<std::string> firedTriggers;
        std::set<std::string> enterRulesDone;
    };
    Checkpoint checkpoint_;
    float deadTime_ = 0.0f;
    std::string deadText_; // who died

    int warnHealth_ = 0;    // the values last seen, to notice a drop
    int warnCondition_ = 0;
    float warning_ = 0.0f;  // seconds the note stays up
    std::string warningText_;

    // The bars: the value last shown, and a flash when it changes.
    std::array<int, 3> barValue_{};
    std::array<float, 3> barFlash_{};
    std::array<bool, 3> barDown_{};

    hollow::Shake shake_;
    hollow::Particles particles_;
    std::map<std::string, float> glow_;     // object key -> strength, for fading lights
    std::map<std::string, hollow::Flicker> flicker_;
    std::map<std::string, hollow::Animator> objectAnimators_; // object key -> its animated sprite
    Color tint_{255, 255, 255, 255};
    Color tintFrom_{255, 255, 255, 255};
    Color tintTo_{255, 255, 255, 255};
    float tintTime_ = 0.0f;
    float tintLength_ = 0.0f;

    float timeInRoom_ = 0.0f;
    float idleTime_ = 0.0f;
    float nextIdleBark_ = 24.0f;
    Target picked_; // chosen with W/S; forgotten when the player walks
    float blockedPush_ = 0.0f;
    float blockedCooldown_ = 0.0f;
    float emberTimer_ = 0.0f;
};
