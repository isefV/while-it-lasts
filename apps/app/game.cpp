#include "game.hpp"

#include "scripts/room_script.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

using namespace hollow;

namespace
{
    constexpr float Reach = 6.0f;          // how far beside a thing the player can still use it
    constexpr float RoomFade = 0.45f;      // seconds, each way
    constexpr float LightFade = 1.5f;      // seconds for a fire to die down
    constexpr float FollowGap = 16.0f;     // X walks this far behind the player
    constexpr float DragGap = 26.0f;       // X trails this far behind: his head at the player's hands
    constexpr float DragEnterX = 52.0f;    // entering a room pulling X: at least this far in, so he is on screen
    constexpr float DragSpeed = 0.45f;     // the player's speed while dragging X
    constexpr float DragSwing = 240.0f;    // how fast X swings round when Y turns while dragging him
    constexpr float HaulSpeed = 0.8f;      // Y's speed pulling X into a new room (the walk-in)
    constexpr float BarFlash = 0.8f;       // seconds a bar flashes after it changes

    // Music outside the rooms (assets/sounds/<id>.qoa); rooms name theirs in "music".
    constexpr const char* TitleMusic = "music_title";
    constexpr const char* EndMusic = "music_end";

    // A room's "fog": these two images, fading into each other every FogCycle seconds.
    constexpr const char* FogImages[] = {"backgrounds/fog_1.png", "backgrounds/fog_2.png"};
    constexpr float FogCycle = 9.0f;

    // A room's "frame": foliage over the edges of the scene, 24 x 24 pieces.
    constexpr float FrameTile = 24.0f;
    constexpr const char* FrameCorners[] = {"ui/frame/top_left.png", "ui/frame/top_right.png", "ui/frame/bottom_left.png",
                                            "ui/frame/bottom_right.png"};
    constexpr const char* FrameEdges[] = {"ui/frame/top.png", "ui/frame/bottom.png", "ui/frame/left.png", "ui/frame/right.png"};

    // Screenshots are the 1080p screen: the canvas 4x, the scene 5x.
    constexpr int ShotWidth = 1920;
    constexpr int ShotHeight = 1080;

    // The Reaper's frames, wherever it shows up (reaper.cpp, revive.cpp, the scripts).
    constexpr const char* ReaperSheet = "sprites/reaper.json";

    // Dying, and the warning before it.
    constexpr float DeadWait = 1.5f;   // seconds on the death screen before E works
    constexpr int LowHealth = 2;       // y_health this low (of 5) sounds the alarm
    constexpr int LowCondition = 1;    // x_condition this low (of 3)
    constexpr float WarningTime = 4.5f;

    float number(const std::vector<std::string>& args, std::size_t index, float fallback)
    {
        if (index >= args.size()) return fallback;
        char* end = nullptr;
        const float value = std::strtof(args[index].c_str(), &end);
        return end == args[index].c_str() ? fallback : value;
    }

    std::string hoverSprite(const std::string& sprite)
    {
        const auto dot = sprite.rfind('.');
        return dot == std::string::npos ? sprite + "_hover" : sprite.substr(0, dot) + "_hover" + sprite.substr(dot);
    }

    // What the player can use lights up, slowly breathing: how strongly.
    float selectGlow()
    {
        return 0.24f + 0.10f * std::sin(static_cast<float>(GetTime()) * 4.0f);
    }

    // The selected thing drawn a second time, added on top: it lights up.
    template <typename Draw>
    void drawSelected(Draw draw)
    {
        BeginBlendMode(BLEND_ADDITIVE);
        draw();
        EndBlendMode();
    }
} // namespace

Game::Game(std::string assetsDir)
    : assets_(std::move(assetsDir))
    , audio_(assets_)
{
#ifndef NDEBUG
    // Debug builds: Settings, to compare the fonts in game.json "ui" "fonts".
    titleMenu_.items = {"Begin", "Settings", "Quit"};
    pauseMenu_.items = {"Resume", "Settings", "Quit to title", "Quit game"};
#else
    titleMenu_.items = {"Begin", "Quit"};
    pauseMenu_.items = {"Resume", "Quit to title", "Quit game"};
#endif
}

Game::~Game()
{
    shutdownGraphics();
}

const std::set<std::string, std::less<>>& Game::commands()
{
    static const std::set<std::string, std::less<>> verbs = {
        "goto", "sound", "ambient", "music", "pose", "companion", "place",
        "step", "face", "shake", "tint", "event", "end", "bark", "fight", "revive", "reaper",
    };
    return verbs;
}

const std::vector<std::string>& Game::scriptConversations()
{
    static const std::vector<std::string> ids = {
        "campsite.hurry", "campsite.figure", "checkpoint.spotted", "checkpoint.chase", "summit.reaper",
    };
    return ids;
}

// ---------------------------------------------------------------------------
// Loading
// ---------------------------------------------------------------------------

bool Game::load(Diagnostics& diag)
{
    data_ = loadGameData(assets_.root(), diag);
    if (!diag.ok()) return false; // references into a file that did not load would all be reported again
    checkGameData(data_, commands(), diag);

    for (const auto& id : scriptConversations())
        if (!data_.conversations.contains(id))
            diag.error("apps/app/scripts: the conversation \"" + id + "\" is started by a room script but no dialogue file defines it");
    for (const auto& [id, room] : data_.rooms)
        if (!room.script.empty() && !makeRoomScript(room.script))
            diag.error(room.file + ": " + id + ".script: unknown script \"" + room.script + "\"");
    if (!diag.ok()) return false;

    for (const auto& v : data_.variables)
        state_.define(v.name, v.value, v.min, v.max);
    for (auto& [id, item] : data_.items)
    {
        item.name = replaceTokens(item.name, data_.tokens());
        item.description = replaceTokens(item.description, data_.tokens());
    }
    for (auto& [id, room] : data_.rooms)
        for (auto& object : room.objects)
            object.name = replaceTokens(object.name, data_.tokens());
    runner_ = std::make_unique<DialogueRunner>(data_.conversations, state_, [this](const Effect& e) { onCommand(e); });
    runner_->setTokens(data_.tokens());

    player_.def = &data_.characters.find(data_.player)->second;
    companion_.def = &data_.characters.find(data_.companion)->second;
    return true;
}

void Game::initGraphics()
{
    canvas_.load(layout::CanvasWidth, layout::CanvasHeight);
    scene_.load(static_cast<int>(layout::SceneWidth), static_cast<int>(layout::SceneHeight));
    text_.load(assets_, data_.ui);
    for (std::size_t i = 0; i < data_.ui.fonts.size(); ++i)
        if (data_.ui.fonts[i].file == data_.ui.font) fontIndex_ = i;
    audio_.init();

    const auto loadSheet = [this](const std::string& key, const std::string& config) {
        Diagnostics diag;
        if (auto sheet = loadSpriteSheet(assets_, config, diag)) sheets_[key] = std::move(*sheet);
        for (const auto& e : diag.errors)
            TraceLog(LOG_WARNING, "%s", e.c_str());
    };
    for (const auto& [id, character] : data_.characters)
        loadSheet(id, character.sprite);
    loadSheet("reaper", ReaperSheet);
    const auto sheet = [this](const std::string& id) -> const SpriteSheet* {
        const auto it = sheets_.find(id);
        return it == sheets_.end() ? nullptr : &it->second;
    };
    player_.sheet = sheet(data_.player);
    companion_.sheet = sheet(data_.companion);

    // Objects whose sprite is a sheet config (.json) are animated.
    for (const auto& [id, room] : data_.rooms)
        for (const auto& object : room.objects)
        {
            if (!object.sprite.ends_with(".json") || objectSheets_.contains(object.sprite)) continue;
            Diagnostics diag;
            if (auto loaded = loadSpriteSheet(assets_, object.sprite, diag)) objectSheets_[object.sprite] = std::move(*loaded);
            for (const auto& e : diag.errors)
                TraceLog(LOG_WARNING, "%s", e.c_str());
        }
    if (screen_ == Screen::Title) audio_.setMusic(TitleMusic);
    graphics_ = true;
}

void Game::shutdownGraphics()
{
    if (!graphics_) return;
    script_.reset();
    audio_.shutdown();
    text_.unload();
    scene_.unload();
    canvas_.unload();
    assets_.unloadAll();
    graphics_ = false;
}

// ---------------------------------------------------------------------------
// Rooms
// ---------------------------------------------------------------------------

void Game::newGame(const std::string& room)
{
    state_.reset();
    for (const auto& item : data_.startItems)
        state_.give(item);
    usedOnce_.clear();
    useCount_.clear();
    firedTriggers_.clear();
    enterRulesDone_.clear();
    runner_->stop();
    barks_.stop();
    pendingRoom_.clear();
    ending_ = false;
    for (std::size_t i = 0; i < data_.bars.size() && i < barValue_.size(); ++i)
    {
        barValue_[i] = state_.get(data_.bars[i].var);
        barFlash_[i] = 0.0f;
    }

    screen_ = Screen::Playing;
    fade_.set(1.0f);
    fade_.to(0.0f, 1.2f);
    companionMode_ = CompanionMode::Absent; // a new game starts with nobody in tow
    const std::string first = room.empty() ? data_.startRoom : room;
    saveCheckpoint(first, -1.0f);
    enterRoom(first, -1.0f);
}

void Game::saveCheckpoint(const std::string& room, float spawnX)
{
    checkpoint_.valid = true;
    checkpoint_.room = room;
    checkpoint_.spawnX = spawnX;
    checkpoint_.mode = companionMode_;
    checkpoint_.state = state_.snapshot();
    checkpoint_.usedOnce = usedOnce_;
    checkpoint_.useCount = useCount_;
    checkpoint_.firedTriggers = firedTriggers_;
    checkpoint_.enterRulesDone = enterRulesDone_;
    warnHealth_ = state_.get("y_health");
    warnCondition_ = state_.get("x_condition");
    warning_ = 0.0f;
}

void Game::retry()
{
    // The level again, from its start, with everything as it was then.
    state_.restore(checkpoint_.state);
    usedOnce_ = checkpoint_.usedOnce;
    useCount_ = checkpoint_.useCount;
    firedTriggers_ = checkpoint_.firedTriggers;
    enterRulesDone_ = checkpoint_.enterRulesDone;
    companionMode_ = checkpoint_.mode;
    runner_->stop();
    barks_.stop();
    pendingRoom_.clear();
    ending_ = false;
    for (std::size_t i = 0; i < data_.bars.size() && i < barValue_.size(); ++i)
    {
        barValue_[i] = state_.get(data_.bars[i].var);
        barFlash_[i] = 0.0f;
    }
    warnHealth_ = state_.get("y_health");
    warnCondition_ = state_.get("x_condition");
    warning_ = 0.0f;
    companion_.pose = "idle";
    screen_ = Screen::Playing;
    fade_.set(1.0f);
    fade_.to(0.0f, 1.0f);
    enterRoom(checkpoint_.room, checkpoint_.spawnX);
}

void Game::checkDeath()
{
    // Not in the middle of a room change, a fight (Y goes down first) or a
    // near-death scene (it plays out).
    if (!pendingRoom_.empty() || ending_ || haul_.active || fight_.active() || revive_.active()) return;
    const bool robDead = state_.get("y_health") <= 0;
    const bool jaceDead = state_.get("x_condition") <= 0;
    if (!robDead && !jaceDead) return;

    const auto name = [this](const std::string& id) {
        const auto it = data_.characters.find(id);
        return it == data_.characters.end() ? id : it->second.name;
    };
    if (robDead)
    {
        deadText_ = name(data_.player) + " is dead.";
        player_.pose = "lying";
    }
    else
    {
        deadText_ = name(data_.companion) + " is gone.";
        if (companion_.visible) companion_.pose = "lying";
    }
    runner_->stop();
    barks_.stop();
    player_.stop();
    audio_.setMusic({});
    audio_.setAmbient({});
    audio_.play("sting");
    screen_ = Screen::Dead;
    deadTime_ = 0.0f;
}

void Game::updateDead(float dt)
{
    deadTime_ += dt;
    shake_.update(dt, random_);
    if (deadTime_ < DeadWait) return;
    if (input_.pressed(Action::Confirm)) retry();
    else if (input_.pressed(Action::Pause)) toTitle();
}

void Game::updateWarnings(float dt)
{
    warning_ = std::max(0.0f, warning_ - dt);
    const int health = state_.get("y_health");
    const int condition = state_.get("x_condition");
    const auto name = [this](const std::string& id) {
        const auto it = data_.characters.find(id);
        return it == data_.characters.end() ? id : it->second.name;
    };
    std::string text;
    if (health < warnHealth_ && health > 0 && health <= LowHealth)
        text = name(data_.player) + " is badly hurt. Tab: bandages, painkillers.";
    else if (condition < warnCondition_ && condition > 0 && condition <= LowCondition)
        text = name(data_.companion) + " is fading. Tab: bandages, painkillers.";
    warnHealth_ = health;
    warnCondition_ = condition;
    if (text.empty()) return;

    warningText_ = text;
    warning_ = WarningTime;
    shake_.start(0.5f, 2.5f);
    audio_.play("alarm", 0.8f);
}

bool Game::startAt(const std::string& room, const std::vector<std::pair<std::string, int>>& values)
{
    if (!room.empty() && !data_.rooms.contains(room))
    {
        TraceLog(LOG_ERROR, "Hollow: --room: no room \"%s\"", room.c_str());
        return false;
    }
    newGame(room);
    for (const auto& [name, value] : values)
    {
        if (!state_.defined(name)) TraceLog(LOG_WARNING, "Hollow: --set: no variable \"%s\"", name.c_str());
        state_.set(name, value);
    }
    saveCheckpoint(room_->id, -1.0f); // with the values set
    fade_.set(0.0f);
    return true;
}

bool Game::saveCanvas(const std::string& path)
{
    RenderTexture2D shot = LoadRenderTexture(ShotWidth, ShotHeight);
    BeginTextureMode(shot);
    canvas_.compose(ShotWidth, ShotHeight, sceneShown_ ? &scene_ : nullptr, sceneAt_);
    EndTextureMode();
    Image image = LoadImageFromTexture(shot.texture);
    UnloadRenderTexture(shot);
    ImageFlipVertical(&image);
    ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8); // as on screen: over black, no see-through pixels
    const bool saved = ExportImage(image, path.c_str());
    UnloadImage(image);
    return saved;
}

void Game::enterRoom(const std::string& id, float spawnX)
{
    const bool hauling = companionMode_ == CompanionMode::Drag; // still pulling X from the last room
    haul_ = HaulIn{};
    room_ = &data_.rooms.find(id)->second;
    script_ = makeRoomScript(room_->script);

    timeInRoom_ = 0.0f;
    idleTime_ = 0.0f;
    nextIdleBark_ = random_.uniform(18.0f, 28.0f);
    particles_.clear();
    glow_.clear();
    barks_.stop();
    insideTriggers_.clear();
    picked_ = {};
    blockedPush_ = 0.0f;
    fight_.stop();
    revive_.stop();
    reaper_.reset();
    state_.set("fighting", 0);

    player_.x = spawnX >= 0.0f ? spawnX : room_->playerX;
    player_.ground = layout::GroundY;
    player_.facing = 1;
    player_.pose = "idle";
    player_.speedScale = 1.0f;
    player_.stop();

    const CompanionDef& c = room_->companion;
    companion_.ground = layout::GroundY;
    companion_.facing = 1;
    if (!c.present)
    {
        setCompanionMode(CompanionMode::Absent);
    }
    else if (hauling)
    {
        startHaulIn();
    }
    else if (c.mode == "follow")
    {
        companion_.x = std::max(room_->walkMin, player_.x - FollowGap);
        setCompanionMode(CompanionMode::Follow);
    }
    else if (c.mode == "drag")
    {
        // Still pulling him from the last room: he comes in behind, in view.
        player_.x = std::max(player_.x, std::min(DragEnterX, room_->walkMax));
        setCompanionMode(CompanionMode::Drag);
    }
    else
    {
        companion_.pose = c.pose;
        setCompanionMode(CompanionMode::Stay, c.x);
    }

    tint_ = tintFrom_ = tintTo_ = room_->tint;
    tintLength_ = 0.0f;
    audio_.setAmbient(room_->ambient);
    audio_.setMusic(room_->music);

    // Lights that are burning when the room opens start at full strength.
    for (const auto& object : room_->objects)
        if (!object.effect.empty() && present(object)) glow_[objectKey(object)] = 1.0f;

    if (script_) script_->enter(*this);
    if (!haul_.active) runEnterRules();
}

void Game::startHaulIn()
{
    // Y comes in from the edge with X in tow. Where X waits in this room (or
    // in the middle, in a room where he would walk) Y lets go and X sits;
    // where the dragging goes on, Y keeps hold. The room's conversation
    // starts after that (updateHaulIn).
    const CompanionDef& c = room_->companion;
    haul_ = HaulIn{};
    haul_.active = true;
    player_.x = room_->walkMin;
    setCompanionMode(CompanionMode::Drag); // X behind him, off the edge

    float target = std::max(room_->playerX, std::min(DragEnterX, room_->walkMax));
    if (c.mode != "drag")
    {
        const bool waits = c.mode == "stay" && c.x >= 0.0f;
        target = (waits ? c.x : layout::SceneWidth / 2.0f) + DragGap;
        haul_.release = true;
        haul_.returnX = waits ? room_->playerX : -1.0f;
        haul_.pose = waits && !c.pose.empty() ? c.pose : "sit";
    }
    player_.walkTo(target);
}

void Game::updateHaulIn()
{
    if (player_.walking()) return;
    if (haul_.release && companionMode_ == CompanionMode::Drag)
    {
        setCompanionMode(CompanionMode::Stay, companion_.x);
        companion_.pose = haul_.pose;
        audio_.play("dirt", 0.4f);
        if (haul_.returnX >= 0.0f && std::fabs(player_.x - haul_.returnX) > 1.0f) player_.walkTo(haul_.returnX);
        return;
    }
    player_.facing = companion_.x >= player_.x ? 1 : -1;
    haul_.active = false;
    runEnterRules();
}

void Game::runEnterRules()
{
    // Every matching rule applies its effects; the first one with a
    // conversation starts it.
    bool talking = false;
    for (std::size_t i = 0; i < room_->onEnter.size(); ++i)
    {
        const EnterRule& rule = room_->onEnter[i];
        const std::string key = room_->id + "#" + std::to_string(i);
        if (rule.once && enterRulesDone_.contains(key)) continue;
        if (!rule.condition.evaluate(state_)) continue;
        if (!rule.dialogue.empty() && talking) continue;

        enterRulesDone_.insert(key);
        applyEffects(rule.effects, state_, [this](const Effect& e) { onCommand(e); });
        if (!rule.dialogue.empty()) talking = startDialogue(rule.dialogue);
    }
}

void Game::setCompanionMode(CompanionMode mode, float x)
{
    const CompanionMode before = companionMode_;
    companionMode_ = mode;
    state_.set("dragging", mode == CompanionMode::Drag ? 1 : 0);
    state_.set("x_here", mode == CompanionMode::Absent ? 0 : 1);
    companion_.visible = mode != CompanionMode::Absent;
    companion_.stop();

    if (before == CompanionMode::Drag && mode != CompanionMode::Drag)
    {
        player_.pose = "idle";
        player_.speedScale = 1.0f;
        if (companion_.pose == "dragged") companion_.pose = "sit";
    }

    switch (mode)
    {
    case CompanionMode::Stay:
        if (x >= 0.0f) companion_.x = x;
        break;
    case CompanionMode::Follow:
        companion_.pose = "idle";
        break;
    case CompanionMode::Drag:
        player_.pose = "drag";
        player_.speedScale = DragSpeed;
        companion_.pose = "dragged";
        dragSide_ = -player_.facing;
        companion_.x = player_.x + static_cast<float>(dragSide_) * DragGap;
        break;
    case CompanionMode::Absent: break;
    }
}

void Game::goTo(const std::string& room, float x)
{
    if (!pendingRoom_.empty() || ending_ || !data_.rooms.contains(room)) return;
    pendingRoom_ = room;
    pendingX_ = x;
    fade_.to(1.0f, RoomFade);
}

void Game::endGame()
{
    if (ending_) return;
    ending_ = true;
    fade_.to(1.0f, 3.0f);
}

void Game::setTint(Color tint, float seconds)
{
    tintFrom_ = tint_;
    tintTo_ = tint;
    tintTime_ = 0.0f;
    tintLength_ = seconds;
    if (seconds <= 0.0f) tint_ = tint;
}

std::string Game::objectKey(const ObjectDef& object) const
{
    return room_->id + "/" + object.id;
}

bool Game::present(const ObjectDef& object) const
{
    return object.condition.evaluate(state_);
}

const ObjectDef* Game::coverAt(float x) const
{
    for (const auto& object : room_->objects)
        if (object.cover && present(object) && x >= object.rect.x && x <= object.rect.x + object.rect.width)
            return &object;
    return nullptr;
}

Actor* Game::actor(const std::string& id)
{
    if (id == data_.player) return &player_;
    if (id == data_.companion) return &companion_;
    return nullptr;
}

// ---------------------------------------------------------------------------
// Dialogue and commands
// ---------------------------------------------------------------------------

bool Game::dialogueActive() const
{
    return runner_ && runner_->active();
}

bool Game::busy() const
{
    return dialogueActive() || !pendingRoom_.empty() || inventoryPanel_.isOpen() || ending_ || revive_.active() ||
           haul_.active;
}

bool Game::startDialogue(const std::string& id)
{
    barks_.stop();
    input_.consume();
    const bool started = runner_->start(id);
    if (!started && !data_.conversations.contains(id))
        TraceLog(LOG_WARNING, "Hollow: no conversation \"%s\"", id.c_str());
    return started;
}

void Game::bark(const std::string& pool)
{
    const auto it = data_.barks.find(pool);
    if (it == data_.barks.end() || it->second.entries.empty() || !it->second.condition.evaluate(state_)) return;

    const auto& entries = it->second.entries;
    const int index = barkPickers_[pool].pick(static_cast<int>(entries.size()), random_);
    std::vector<BarkLine> lines = entries[static_cast<std::size_t>(index)];
    const TokenMap tokens = data_.tokens();
    for (auto& line : lines)
    {
        const Actor* who = actor(line.speaker);
        if (!who || !who->visible) return; // the speaker is not here
        line.text = replaceTokens(line.text, tokens);
    }
    barks_.play(std::move(lines));
}

void Game::onCommand(const Effect& effect)
{
    const auto& args = effect.args;
    const auto arg = [&](std::size_t i) { return i < args.size() ? args[i] : std::string(); };
    const std::string& verb = effect.name;

    if (verb == "goto")
    {
        goTo(arg(0), number(args, 1, -1.0f));
    }
    else if (verb == "sound")
    {
        audio_.play(arg(0));
    }
    else if (verb == "ambient")
    {
        audio_.setAmbient(args);
    }
    else if (verb == "music")
    {
        audio_.setMusic(arg(0) == "stop" ? std::string() : arg(0));
    }
    else if (verb == "pose")
    {
        if (Actor* who = actor(arg(0))) who->pose = arg(1);
    }
    else if (verb == "companion")
    {
        const std::string mode = arg(0);
        const float x = number(args, 1, -1.0f);
        if (mode == "follow") setCompanionMode(CompanionMode::Follow);
        else if (mode == "drag") setCompanionMode(CompanionMode::Drag);
        else if (mode == "absent") setCompanionMode(CompanionMode::Absent);
        else setCompanionMode(CompanionMode::Stay, x);
    }
    else if (verb == "place")
    {
        if (Actor* who = actor(arg(0)))
        {
            who->x = number(args, 1, who->x);
            who->stop();
        }
    }
    else if (verb == "step")
    {
        if (Actor* who = actor(arg(0)))
            who->walkTo(std::clamp(who->x + number(args, 1, 0.0f), room_->walkMin, room_->walkMax));
    }
    else if (verb == "face")
    {
        if (Actor* who = actor(arg(0))) who->facing = arg(1) == "left" ? -1 : 1;
    }
    else if (verb == "shake")
    {
        shake_.start(number(args, 0, 0.3f), number(args, 1, 2.0f));
    }
    else if (verb == "tint")
    {
        if (const auto color = parseColor(arg(0))) setTint(*color, number(args, 1, 0.0f));
    }
    else if (verb == "event")
    {
        if (script_) script_->event(*this, arg(0), std::vector<std::string>(args.size() > 1 ? args.begin() + 1 : args.end(), args.end()));
    }
    else if (verb == "end")
    {
        endGame();
    }
    else if (verb == "bark")
    {
        bark(arg(0));
    }
    else if (verb == "fight")
    {
        if (room_->fight.present) fight_.start(*this);
        else TraceLog(LOG_WARNING, "Hollow: fight: the room \"%s\" has no \"fight\"", room_->id.c_str());
    }
    else if (verb == "revive")
    {
        revive_.start(*this, arg(0), static_cast<int>(number(args, 1, 8.0f)), number(args, 2, 14.0f));
    }
    else if (verb == "reaper")
    {
        reaper_.command(*this, args);
    }
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void Game::frame(float dt)
{
    dt = std::min(dt, 0.1f); // a stall (window drag) must not skip a timer
    update(dt);
    draw();
}

void Game::update(float dt)
{
    input_.update();
    if (input_.pressed(Action::Fullscreen) || (IsKeyDown(KEY_LEFT_ALT) && IsKeyPressed(KEY_ENTER)))
    {
        ToggleBorderlessWindowed();
        input_.consume();
    }
    audio_.update(dt);

    switch (screen_)
    {
    case Screen::Title:
    {
        const int choice = titleMenu_.update(input_);
        const std::string picked = choice >= 0 ? titleMenu_.items[choice] : std::string();
        if (picked == "Begin") newGame();
        else if (picked == "Settings") openSettings();
        else if (picked == "Quit" || input_.pressed(Action::Pause)) quit_ = true;
        break;
    }
    case Screen::Paused:
    {
        if (input_.pressed(Action::Pause))
        {
            screen_ = Screen::Playing;
            break;
        }
        const int choice = pauseMenu_.update(input_);
        const std::string picked = choice >= 0 ? pauseMenu_.items[choice] : std::string();
        if (picked == "Resume")
        {
            screen_ = Screen::Playing;
        }
        else if (picked == "Settings")
        {
            openSettings();
        }
        else if (picked == "Quit to title")
        {
            runner_->stop();
            script_.reset();
            audio_.setAmbient({});
            toTitle();
        }
        else if (picked == "Quit game")
        {
            quit_ = true;
        }
        break;
    }
    case Screen::Settings: updateSettings(); break;
    case Screen::Playing: updatePlaying(dt); break;
    case Screen::Dead: updateDead(dt); break;
    case Screen::EndCard:
        endCardTime_ += dt;
        if (endCardTime_ > 4.0f && input_.pressed(Action::Confirm)) toTitle();
        break;
    }
}

void Game::toTitle()
{
    screen_ = Screen::Title;
    titleMenu_.selected = 0;
    audio_.setMusic(TitleMusic);
}

void Game::openSettings()
{
    settingsFrom_ = screen_;
    screen_ = Screen::Settings;
    settingsMenu_.selected = 0;
    useFont(fontIndex_); // writes the menu's labels
}

void Game::useFont(std::size_t index)
{
    const std::vector<FontChoice>& fonts = data_.ui.fonts;
    if (index < fonts.size() && index != fontIndex_)
    {
        fontIndex_ = index;
        text_.load(assets_, fonts[index].file, fonts[index].size);
    }
    const std::string name = fontIndex_ < fonts.size() ? fonts[fontIndex_].name : std::string("(none in game.json)");
    settingsMenu_.items = {"Font: " + name, "Back"};
}

bool Game::setFont(const std::string& name)
{
    for (std::size_t i = 0; i < data_.ui.fonts.size(); ++i)
        if (data_.ui.fonts[i].name == name)
        {
            useFont(i);
            return true;
        }
    return false;
}

void Game::updateSettings()
{
    // E (or Left / Right on the font) goes to the next font; Esc or Back returns.
    const int choice = settingsMenu_.update(input_);
    const std::size_t count = data_.ui.fonts.size();
    if (settingsMenu_.selected == 0 && count > 0 && (input_.pressed(Action::Left) || input_.pressed(Action::Right)))
        useFont((fontIndex_ + (input_.pressed(Action::Left) ? count - 1 : 1)) % count);
    else if (choice == 0 && count > 0)
        useFont((fontIndex_ + 1) % count);
    else if (choice == 1 || input_.pressed(Action::Pause))
        screen_ = settingsFrom_;
}

void Game::updatePlaying(float dt)
{
    fade_.update(dt);
    shake_.update(dt, random_);
    if (tintLength_ > 0.0f && tintTime_ < tintLength_)
    {
        tintTime_ = std::min(tintTime_ + dt, tintLength_);
        tint_ = mix(tintFrom_, tintTo_, smoothstep(tintTime_ / tintLength_));
    }

    // The room changes while the screen is black.
    if (!pendingRoom_.empty() && fade_.done() && fade_.value() >= 1.0f)
    {
        const std::string id = pendingRoom_;
        pendingRoom_.clear();
        saveCheckpoint(id, pendingX_); // dying here starts this level again
        enterRoom(id, pendingX_);
        fade_.to(0.0f, RoomFade);
    }
    if (ending_ && fade_.done() && fade_.value() >= 1.0f)
    {
        runner_->stop();
        script_.reset();
        audio_.setAmbient({});
        audio_.setMusic(EndMusic);
        screen_ = Screen::EndCard;
        endCardTime_ = 0.0f;
        return;
    }

    timeInRoom_ += dt;

    if (input_.pressed(Action::Pause) && !inventoryPanel_.isOpen())
    {
        screen_ = Screen::Paused;
        pauseMenu_.selected = 0;
        return;
    }

    // Y slows down when hurt; more when dragging X.
    {
        const int health = state_.get("y_health");
        const float pace = health <= 1 ? 0.75f : health <= 2 ? 0.88f : 1.0f;
        player_.speedScale = (companionMode_ == CompanionMode::Drag ? DragSpeed : 1.0f) * pace;
        if (haul_.active) player_.speedScale = companionMode_ == CompanionMode::Drag ? HaulSpeed : 1.0f;
    }

    if (inventoryPanel_.isOpen())
    {
        if (const ItemDef* item = inventoryPanel_.update(inventory(), input_)) startDialogue(item->use);
    }
    else if (dialogueActive())
    {
        const DialogueView& view = runner_->view();
        DialogueBox::Speaker speaker;
        if (view.style == LineStyle::Speech)
        {
            if (const auto it = data_.characters.find(view.speaker); it != data_.characters.end())
                speaker = DialogueBox::Speaker{it->second.name, it->second.textColor};
        }
        const auto result = dialogueBox_.update(view, speaker, input_, dt, text_, data_.ui);
        if (result.choiceShown) sound("choice", 0.8f);
        if (result.advance) runner_->advance();
        else if (result.choice >= 0) runner_->choose(result.choice);
        // A timed choice's clock starts once it can be picked.
        runner_->update(dialogueBox_.choiceLocked() ? 0.0f : dt);
    }
    else if (revive_.active())
    {
        revive_.update(*this, dt);
    }
    else if (haul_.active)
    {
        updateHaulIn();
    }
    else if (pendingRoom_.empty() && !ending_)
    {
        if (fight_.active()) fight_.update(*this, dt);
        if (!dialogueActive() && !fight_.down()) updateExploring(dt);
    }

    updateCompanion(dt);
    reaper_.update(*this, dt);

    // A bar that changed flashes for a moment.
    for (std::size_t i = 0; i < data_.bars.size() && i < barValue_.size(); ++i)
    {
        barFlash_[i] = std::max(0.0f, barFlash_[i] - dt);
        const int value = state_.get(data_.bars[i].var);
        if (value == barValue_[i]) continue;
        barDown_[i] = value < barValue_[i];
        barValue_[i] = value;
        barFlash_[i] = BarFlash;
    }
    player_.update(dt);
    companion_.update(dt);

    // Footsteps in time with the feet (the room's "steps"), X's quieter.
    if (!room_->steps.empty())
    {
        const auto stepSound = [this] {
            return room_->steps[static_cast<std::size_t>(random_.range(0, static_cast<int>(room_->steps.size()) - 1))];
        };
        if (player_.footfall()) audio_.play(stepSound(), 0.55f, random_.uniform(0.9f, 1.1f));
        if (companion_.visible && companion_.footfall()) audio_.play(stepSound(), 0.32f, random_.uniform(0.85f, 1.0f));
    }

    // A cigarette smoking: a thin grey wisp from its tip.
    for (const Actor* who : {&player_, &companion_})
        if (const auto tip = who->emitPoint(); tip && random_.chance(dt * 5.0f))
        {
            Particles::Style smoke;
            smoke.color = Color{168, 172, 178, 255};
            smoke.velocityMin = Vector2{-2, -9};
            smoke.velocityMax = Vector2{3, -5};
            smoke.life = 1.6f;
            particles_.emit(*tip, 1, smoke, random_);
        }

    for (const auto& object : room_->objects)
        if (const SpriteSheet* sheet = objectSheet(object)) objectAnimators_[objectKey(object)].update(dt, sheet);
    barks_.update(dt);
    particles_.update(dt);

    // Lights follow their object's "if": a fire put out fades instead of vanishing.
    for (const auto& object : room_->objects)
    {
        if (object.effect.empty()) continue;
        const std::string key = objectKey(object);
        const bool on = present(object);
        float& strength = glow_[key];
        strength = approach(strength, on ? 1.0f : 0.0f, dt / LightFade);
        flicker_[key].update(dt, random_);

        if (strength <= 0.0f) continue;
        const Vector2 base{object.rect.x + object.rect.width / 2.0f, object.rect.y + object.rect.height - 2.0f};
        Particles::Style style;
        style.color = data_.ui.accent;
        if (object.effect == "fire" && random_.chance(dt * 7.0f * strength))
            particles_.emit(base, 1, style, random_);
        if (object.effect == "ember" && random_.chance(dt * 1.2f * strength))
        {
            style.velocityMin = Vector2{-2, -8};
            style.velocityMax = Vector2{3, -3};
            particles_.emit(Vector2{base.x, object.rect.y}, 1, style, random_);
        }
    }

    if (script_) script_->update(*this, dt);
    updateWarnings(dt);
    checkDeath();
}

void Game::updateExploring(float dt)
{
    if (input_.pressed(Action::Inventory) && !fight_.active())
    {
        inventoryPanel_.open();
        input_.consume();
        return;
    }

    // Walking, stopped by the room's edges and by active blocks.
    float dir = 0.0f;
    if (input_.down(Action::Left)) dir -= 1.0f;
    if (input_.down(Action::Right)) dir += 1.0f;
    if (player_.walking()) dir = 0.0f; // walking on its own (a cutscene step)

    float minX = room_->walkMin;
    float maxX = room_->walkMax;
    const ObjectDef* blockLeft = nullptr;
    const ObjectDef* blockRight = nullptr;
    for (const auto& object : room_->objects)
    {
        if (!object.hasBlock || !present(object) || !object.block.evaluate(state_)) continue;
        if (player_.x <= object.rect.x)
        {
            if (object.rect.x - 1.0f < maxX)
            {
                maxX = object.rect.x - 1.0f;
                blockRight = &object;
            }
        }
        else if (player_.x >= object.rect.x + object.rect.width)
        {
            if (object.rect.x + object.rect.width + 1.0f > minX)
            {
                minX = object.rect.x + object.rect.width + 1.0f;
                blockLeft = &object;
            }
        }
    }

    const float dx = dir * player_.speed() * dt;
    const float nx = std::clamp(player_.x + dx, minX, maxX);

    const ObjectDef* pushing = nullptr;
    if (dir > 0.0f && player_.x + dx > maxX) pushing = blockRight;
    if (dir < 0.0f && player_.x + dx < minX) pushing = blockLeft;
    blockedCooldown_ -= dt;
    if (pushing)
    {
        blockedPush_ += dt;
        if (blockedPush_ > 0.3f && blockedCooldown_ <= 0.0f && !pushing->blocked.empty())
        {
            blockedPush_ = 0.0f;
            blockedCooldown_ = 4.0f;
            startDialogue(pushing->blocked);
            return;
        }
    }
    else
    {
        blockedPush_ = 0.0f;
    }

    if (dir != 0.0f)
    {
        player_.facing = dir > 0.0f ? 1 : -1;
        idleTime_ = 0.0f;
        picked_ = {};
    }
    player_.step(nx - player_.x);

    if (input_.pressed(Action::Grab))
    {
        grabOrRelease();
        if (busy()) return;
    }
    if (fight_.active()) return; // no using things in the middle of a fight

    // Using things: E for the one the player stands next to; W/S pick
    // another when several are in reach.
    const std::vector<Target> inReach = targetsInReach();
    const bool next = input_.pressed(Action::Down);
    if (inReach.size() > 1 && (next || input_.pressed(Action::Up)))
    {
        const int count = static_cast<int>(inReach.size());
        const int index = static_cast<int>(std::find(inReach.begin(), inReach.end(), selectedTarget()) - inReach.begin());
        picked_ = inReach[static_cast<std::size_t>((index + (next ? 1 : count - 1)) % count)];
    }
    if (input_.pressed(Action::Interact))
    {
        if (const Target target = selectedTarget(); target.valid())
        {
            if (target.companion) talkToCompanion();
            else interact(target.object);
            return;
        }
    }

    updateTriggers();
    if (busy()) return;

    // Standing still for a while: someone says something.
    idleTime_ += dt;
    if (idleTime_ >= nextIdleBark_ && !barks_.active() && !room_->idleBarks.empty())
    {
        idleTime_ = 0.0f;
        nextIdleBark_ = random_.uniform(20.0f, 32.0f);
        bark(room_->idleBarks[static_cast<std::size_t>(random_.range(0, static_cast<int>(room_->idleBarks.size()) - 1))]);
    }
}

void Game::updateCompanion(float dt)
{
    switch (companionMode_)
    {
    case CompanionMode::Follow:
    {
        const float target = std::clamp(player_.x - static_cast<float>(player_.facing) * FollowGap, room_->walkMin,
                                        room_->walkMax);
        const int condition = std::clamp(state_.get("x_condition"), 0, 3);
        // A little slower than the player, and a little slower again when he is worse.
        companion_.speedScale = 0.84f + 0.16f * static_cast<float>(condition) / 3.0f;
        if (std::fabs(companion_.x - target) > 8.0f) companion_.walkTo(target);
        // He keeps low when Y does (sneaking at the checkpoint).
        companion_.pose = player_.pose == "kneel" || player_.pose == "crouch" ? player_.pose : "idle";
        break;
    }
    case CompanionMode::Drag:
    {
        // X trails on the side the player is walking away from. Turning round
        // swings him over quickly, his head always toward Y's hands.
        if (player_.moved()) dragSide_ = -player_.facing;
        const float target = player_.x + static_cast<float>(dragSide_) * DragGap;
        companion_.x = approach(companion_.x, target, DragSwing * dt);
        companion_.facing = companion_.x <= player_.x ? 1 : -1;
        companion_.pose = "dragged";
        break;
    }
    default: break;
    }
}

void Game::updateTriggers()
{
    for (const auto& object : room_->objects)
    {
        if (!object.trigger) continue;
        const std::string key = objectKey(object);
        const bool inside =
            present(object) && player_.x >= object.rect.x && player_.x <= object.rect.x + object.rect.width;
        if (!inside)
        {
            insideTriggers_.erase(key);
            continue;
        }
        if (insideTriggers_.contains(key)) continue;
        insideTriggers_.insert(key);
        if (!object.repeat && firedTriggers_.contains(key)) continue;

        firedTriggers_.insert(key);
        if (!object.event.empty() && script_) script_->event(*this, object.event, {});
        if (!object.dialogue.empty()) startDialogue(object.dialogue);
        return;
    }
}

void Game::interact(const ObjectDef* object)
{
    const std::string key = objectKey(*object);
    player_.facing = object->rect.x + object->rect.width / 2.0f >= player_.x ? 1 : -1;
    input_.consume();

    if (!object->exitTo.empty())
    {
        if (object->exitCondition.evaluate(state_))
            goTo(object->exitTo, object->spawnX);
        else if (!object->locked.empty())
            startDialogue(object->locked);
        return;
    }

    const int count = useCount_[key]++;
    if (object->once) usedOnce_.insert(key);
    if (!object->event.empty() && script_) script_->event(*this, object->event, {});
    const std::string& conversation = count > 0 && !object->again.empty() ? object->again : object->dialogue;
    if (!conversation.empty()) startDialogue(conversation);
}

void Game::talkToCompanion()
{
    const CompanionDef& c = room_->companion;
    player_.facing = companion_.x >= player_.x ? 1 : -1;
    input_.consume();

    const std::string key = room_->id + "/@companion";
    const int count = useCount_[key]++;
    const std::string& conversation = count > 0 && !c.again.empty() ? c.again : c.dialogue;
    if (!conversation.empty())
        startDialogue(conversation);
    else if (!c.bark.empty())
        bark(c.bark);
}

bool Game::canGrab() const
{
    return companionMode_ == CompanionMode::Stay && companion_.visible && state_.get("x_down") != 0 &&
           inReach(companion_.bounds());
}

void Game::grabOrRelease()
{
    // G lets go of X where he is, or takes hold of him again. Only while he
    // cannot walk ("x_down").
    const CompanionDef& c = room_->companion;
    if (companionMode_ == CompanionMode::Drag)
    {
        setCompanionMode(CompanionMode::Stay, companion_.x);
        companion_.pose = "lying";
        state_.set("x_down", 1);
        audio_.play("dirt", 0.5f);
        if (!c.release.empty()) startDialogue(c.release);
    }
    else if (canGrab())
    {
        player_.facing = companion_.x >= player_.x ? 1 : -1;
        setCompanionMode(CompanionMode::Drag);
        if (!c.grab.empty()) startDialogue(c.grab);
    }
}

std::vector<InventoryEntry> Game::inventory() const
{
    std::vector<InventoryEntry> entries;
    for (const auto& id : data_.itemOrder)
    {
        const ItemDef& item = data_.items.find(id)->second;
        if (!item.countVar.empty())
        {
            const int count = state_.get(item.countVar);
            if (count > 0) entries.push_back(InventoryEntry{&item, count});
        }
        else if (state_.hasItem(id))
        {
            entries.push_back(InventoryEntry{&item, 0});
        }
    }
    return entries;
}

// ---------------------------------------------------------------------------
// What the player can use
// ---------------------------------------------------------------------------

bool Game::usable(const ObjectDef& object) const
{
    return object.interactive && present(object) && !(object.once && usedOnce_.contains(objectKey(object)));
}

bool Game::companionUsable() const
{
    const CompanionDef& c = room_->companion;
    return companionMode_ != CompanionMode::Absent && companionMode_ != CompanionMode::Drag &&
           (!c.dialogue.empty() || !c.bark.empty());
}

Rectangle Game::targetRect(const Target& target) const
{
    return target.companion ? companion_.bounds() : target.object->rect;
}

bool Game::inReach(Rectangle rect) const
{
    return player_.x >= rect.x - Reach && player_.x <= rect.x + rect.width + Reach;
}

std::vector<Game::Target> Game::targetsInReach() const
{
    std::vector<std::pair<float, Target>> found;
    const auto consider = [&](Target t) {
        const Rectangle r = targetRect(t);
        if (inReach(r)) found.emplace_back(std::fabs(r.x + r.width / 2.0f - player_.x), t);
    };
    for (const auto& object : room_->objects)
        if (usable(object)) consider(Target{&object, false});
    if (companionUsable()) consider(Target{nullptr, true});

    std::stable_sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<Target> targets;
    for (const auto& [distance, t] : found)
        targets.push_back(t);
    return targets;
}

Game::Target Game::selectedTarget() const
{
    const std::vector<Target> targets = targetsInReach();
    if (targets.empty()) return {};
    if (picked_.valid() && std::find(targets.begin(), targets.end(), picked_) != targets.end()) return picked_;
    return targets.front();
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void Game::draw()
{
    const bool fromGame = screen_ == Screen::Settings && settingsFrom_ == Screen::Paused;
    const bool inGame = screen_ == Screen::Playing || screen_ == Screen::Paused || screen_ == Screen::Dead || fromGame;
    if (inGame) drawScene();

    // The scene is shown under the canvas, which is see-through over it.
    sceneShown_ = inGame;
    sceneAt_ = layout::SceneBox;
    if (screen_ != Screen::Settings)
    {
        const Vector2 offset = shake_.offset();
        sceneAt_.x += offset.x * layout::SceneScale;
        sceneAt_.y += offset.y * layout::SceneScale;
    }

    canvas_.begin();
    ClearBackground(inGame ? BLANK : BLACK);
    switch (screen_)
    {
    case Screen::Title: drawTitle(); break;
    case Screen::Playing:
    case Screen::Paused:
        drawSceneLabels();
        drawHud();
        drawWarning();
        fade_.draw(Rectangle{0, 0, static_cast<float>(layout::CanvasWidth), static_cast<float>(layout::CanvasHeight)});
        if (screen_ == Screen::Paused) drawPause();
        break;
    case Screen::Dead: drawDead(); break;
    case Screen::Settings:
        if (fromGame) drawHud();
        drawSettings();
        break;
    case Screen::EndCard: drawEndCard(); break;
    }
    canvas_.end();

    BeginDrawing();
    canvas_.present(sceneShown_ ? &scene_ : nullptr, sceneAt_);
    EndDrawing();
}

Vector2 Game::toCanvas(Vector2 scenePoint) const
{
    return Vector2{sceneAt_.x + scenePoint.x * layout::SceneScale, sceneAt_.y + scenePoint.y * layout::SceneScale};
}

void Game::drawScene()
{
    scene_.begin();
    ClearBackground(BLACK);

    if (const Texture2D* background = assets_.texture(room_->background))
    {
        // See-through parts of the art (a window) show the room's colour, and
        // what stands behind the background (the Reaper out in the yard).
        ClearBackground(multiply(room_->color, tint_));
        for (const auto& object : room_->objects)
            if (object.behind && !object.hidden && present(object)) drawObject(object, false);
        const auto w = static_cast<float>(background->width);
        const auto h = static_cast<float>(background->height);
        DrawTexturePro(*background, Rectangle{0, 0, room_->flip ? -w : w, h}, Rectangle{0, 0, w, h}, Vector2{0, 0}, 0.0f,
                       tint_);
    }
    else
    {
        // Placeholder: the room's colour, a darker band below the ground line.
        DrawRectangle(0, 0, 320, 180, multiply(room_->color, tint_));
        DrawRectangle(0, static_cast<int>(layout::GroundY), 320, 30,
                      multiply(mix(room_->color, BLACK, 0.45f), tint_));
    }

    // Mist over the ground: two images fading into each other, slowly.
    if (room_->fog > 0.0f)
    {
        const float k = 0.5f + 0.5f * std::sin(timeInRoom_ * 2.0f * PI / FogCycle);
        const Texture2D* fogs[] = {assets_.texture(FogImages[0]), assets_.texture(FogImages[1])};
        if (fogs[0]) DrawTexture(*fogs[0], 0, 0, ColorAlpha(tint_, room_->fog * (1.0f - k)));
        if (fogs[1]) DrawTexture(*fogs[1], 0, 0, ColorAlpha(tint_, room_->fog * k));
    }

    if (script_) script_->drawBack(*this);
    reaper_.draw(*this);

    // Only what the player stands next to is marked, and only that can be used.
    Target highlight;
    if (!busy() && !fight_.active()) highlight = selectedTarget();

    for (const auto& object : room_->objects)
        if (!object.front && !object.behind && !object.hidden && present(object))
            drawObject(object, highlight.object == &object);

    if (script_) script_->drawMiddle(*this);
    fight_.drawBack(*this);
    companion_.draw(tint_);
    if (highlight.companion) drawSelected([&] { companion_.draw(ColorAlpha(WHITE, selectGlow())); });
    player_.draw(tint_);

    for (const auto& object : room_->objects)
        if (object.front && !object.hidden && present(object))
            drawObject(object, highlight.object == &object);

    for (const auto& object : room_->objects)
        if (!object.effect.empty()) drawEffect(object);
    particles_.draw();
    fight_.drawFront(*this);

    if (script_) script_->drawFront(*this);
    revive_.drawScene(*this);
    if (room_->frame) drawFrame();

    scene_.end();
}

void Game::drawFrame()
{
    // The corners, then the edges repeated between them (cut off where they
    // run out): foliage closing in around the scene.
    const float w = layout::SceneWidth;
    const float h = layout::SceneHeight;
    const float t = FrameTile;
    const Vector2 corners[] = {{0, 0}, {w - t, 0}, {0, h - t}, {w - t, h - t}};
    for (int i = 0; i < 4; ++i)
        if (const Texture2D* corner = assets_.texture(FrameCorners[i])) DrawTextureV(*corner, corners[i], tint_);

    const Rectangle edges[] = {{t, 0, w - 2 * t, t}, {t, h - t, w - 2 * t, t}};
    for (int i = 0; i < 2; ++i)
    {
        const Texture2D* edge = assets_.texture(FrameEdges[i]);
        if (!edge) continue;
        SetTextureWrap(*edge, TEXTURE_WRAP_REPEAT);
        const Rectangle& r = edges[i];
        DrawTextureRec(*edge, Rectangle{0, 0, r.width, r.height}, Vector2{r.x, r.y}, tint_);
    }

    // Down each side the two side pieces take turns, the other side's one
    // mirrored, so the 24 px repeat does not show.
    const Texture2D* left = assets_.texture(FrameEdges[2]);
    const Texture2D* right = assets_.texture(FrameEdges[3]);
    if (!left || !right) return;
    int n = 0;
    for (float y = t; y < h - t; y += t, ++n)
    {
        const float part = std::min(t, h - t - y);
        const bool other = n % 2 == 1;
        const float sw = other ? -t : t; // a negative width mirrors it
        DrawTextureRec(other ? *right : *left, Rectangle{0, 0, sw, part}, Vector2{0, y}, tint_);
        DrawTextureRec(other ? *left : *right, Rectangle{0, 0, sw, part}, Vector2{w - t, y}, tint_);
    }
}

void Game::drawSceneLabels()
{
    // Drawn on the canvas, not in the scene, so all text has the same pixels.
    if (const BarkLine* line = barks_.current())
    {
        Actor* who = actor(line->speaker);
        const auto it = data_.characters.find(line->speaker);
        if (who && it != data_.characters.end())
            barks_.draw(text_, toCanvas(Vector2{who->x, who->bounds().y}), it->second.textColor);
    }

    if (busy() || fight_.active()) return;
    const Target highlight = selectedTarget();
    if (!highlight.valid()) return;
    const Rectangle r = targetRect(highlight);
    const std::string& label = highlight.companion ? companion_.def->name : highlight.object->name;
    std::string note;
    const std::vector<Target> targets = targetsInReach();
    if (targets.size() > 1)
    {
        const auto index = std::find(targets.begin(), targets.end(), highlight) - targets.begin();
        note = std::to_string(index + 1) + "/" + std::to_string(targets.size());
    }
    drawPrompt(text_, label, toCanvas(Vector2{r.x + r.width / 2.0f, r.y}), data_.ui, note);
}

const SpriteSheet* Game::reaperSheet() const
{
    const auto it = sheets_.find("reaper");
    return it == sheets_.end() ? nullptr : &it->second;
}

const SpriteSheet* Game::objectSheet(const ObjectDef& object) const
{
    const auto it = objectSheets_.find(object.sprite);
    return it == objectSheets_.end() ? nullptr : &it->second;
}

void Game::drawObject(const ObjectDef& object, bool highlighted)
{
    const SpriteSheet* sheet = objectSheet(object);
    // A "_hover" image beside the sprite, if there is one, is shown while selected.
    const Texture2D* hover = highlighted && !sheet ? assets_.texture(hoverSprite(object.sprite)) : nullptr;

    // Drawn with this tint; true if there was an image to draw.
    const auto drawImage = [&](Color tint) {
        // An animated sprite: its "idle" frames, top-left at the object's.
        if (sheet)
        {
            const auto animation = sheet->animations.find("idle");
            const Vector2 origin = animation == sheet->animations.end() ? Vector2{} : animation->second.origin;
            if (animation != sheet->animations.end() &&
                drawSprite(*sheet, objectAnimators_[objectKey(object)],
                           Vector2{object.rect.x + origin.x * object.scale, object.rect.y + origin.y * object.scale}, false,
                           tint, object.scale))
                return true;
        }
        const Texture2D* texture = assets_.texture(object.sprite);
        if (!texture) return false;
        const Texture2D& shown = hover ? *hover : *texture;
        if (object.scale == 1.0f)
            DrawTexture(shown, static_cast<int>(object.rect.x), static_cast<int>(object.rect.y), tint);
        else
        {
            // Scaled: the bottom stays where the rect's bottom is.
            const float w = std::round(static_cast<float>(shown.width) * object.scale);
            const float h = std::round(static_cast<float>(shown.height) * object.scale);
            const Rectangle dest{std::round(object.rect.x), std::round(object.rect.y + object.rect.height - h), w, h};
            DrawTexturePro(shown, Rectangle{0, 0, static_cast<float>(shown.width), static_cast<float>(shown.height)}, dest,
                           Vector2{0, 0}, 0.0f, tint);
        }
        return true;
    };

    if (!drawImage(ColorAlpha(tint_, object.alpha)))
    {
        DrawRectangleRec(object.rect, multiply(object.color, tint_));
        if (highlighted) drawSelected([&] { DrawRectangleRec(object.rect, ColorAlpha(object.color, selectGlow())); });
        return;
    }
    if (highlighted && !hover) drawSelected([&] { drawImage(ColorAlpha(WHITE, object.alpha * selectGlow())); });
}

void Game::drawEffect(const ObjectDef& object)
{
    const std::string key = objectKey(object);
    const auto it = glow_.find(key);
    const float strength = it == glow_.end() ? 0.0f : it->second;
    if (strength <= 0.0f) return;
    const float f = flicker_[key].value();
    const Color warm = data_.ui.accent;
    const Color hot = mix(warm, Color{255, 230, 160, 255}, 0.6f);
    const float cx = object.rect.x + object.rect.width / 2.0f;
    const float bottom = object.rect.y + object.rect.height;

    if (object.effect == "fire" && objectSheet(object))
    {
        // The flames are in the sprite: only the light.
        drawGlow(Vector2{cx, bottom - 10}, 70.0f * f, warm, strength);
    }
    else if (object.effect == "fire")
    {
        // Three tongues of flame over the logs, then the light.
        const float h = std::round((10.0f + 4.0f * (f - 1.0f) * 10.0f) * strength);
        DrawRectangleRec(Rectangle{std::round(cx - 6), bottom - h * 0.7f - 2, 4, h * 0.7f}, ColorAlpha(warm, strength));
        DrawRectangleRec(Rectangle{std::round(cx - 2), bottom - h - 2, 4, h}, ColorAlpha(hot, strength));
        DrawRectangleRec(Rectangle{std::round(cx + 2), bottom - h * 0.6f - 2, 4, h * 0.6f}, ColorAlpha(warm, strength));
        drawGlow(Vector2{cx, bottom - 6}, 70.0f * f, warm, strength);
    }
    else if (object.effect == "ember" && objectSheet(object))
    {
        // The embers are in the sprite (the fire put out): a low, faint light.
        drawGlow(Vector2{cx, bottom - 6}, 22.0f * f, warm, strength * 0.5f);
    }
    else if (object.effect == "ember")
    {
        DrawRectangleRec(Rectangle{std::round(cx), std::round(object.rect.y), 1, 1}, ColorAlpha(hot, strength * std::min(1.0f, f)));
        drawGlow(Vector2{cx, object.rect.y}, 12.0f * f, warm, strength * 0.8f);
    }
    else
    {
        drawGlow(Vector2{cx, object.rect.y + object.rect.height / 2.0f}, object.rect.width * f, warm, strength);
    }
}

void Game::drawHud()
{
    for (std::size_t i = 0; i < data_.bars.size() && i < 3; ++i)
    {
        // The name, the bar under it; it flashes when it changes.
        const BarDef& bar = data_.bars[i];
        const Rectangle slot = layout::BarSlots[i];
        Color color = bar.color;
        if (barFlash_[i] > 0.0f && std::fmod(barFlash_[i], 0.2f) < 0.1f)
            color = barDown_[i] ? Color{170, 60, 60, 255} : data_.ui.highlight;
        if (!bar.label.empty()) text_.draw(bar.label, Vector2{slot.x, slot.y - layout::LineHeight - 1}, data_.ui.dim);
        drawBar(assets_, slot, state_.get(bar.var), bar.max, color, data_.ui);
    }

    if (inventoryPanel_.isOpen())
    {
        inventoryPanel_.draw(inventory(), text_, data_.ui, assets_);
        return;
    }
    if (dialogueActive())
    {
        dialogueBox_.draw(runner_->view(), text_, data_.ui, assets_);
        return;
    }

    const Rectangle box = layout::DialogueBox;
    drawPanel(assets_, box, data_.ui);
    if (revive_.active())
    {
        revive_.drawHud(*this);
        return;
    }
    if (fight_.active())
    {
        fight_.drawHud(*this);
        return;
    }

    // Between conversations: where we are, and the keys.
    text_.draw(room_->name, Vector2{box.x + layout::Padding, box.y + layout::Padding}, data_.ui.dim);
    std::vector<std::string> parts = {"A/D walk", "E use"};
    if (!busy() && targetsInReach().size() > 1) parts.emplace_back("W/S next");
    if (companionMode_ == CompanionMode::Drag && !busy()) parts.emplace_back("G let go");
    else if (!busy() && canGrab()) parts.emplace_back("G take hold");
    parts.emplace_back("Tab items");
    parts.emplace_back("Esc pause");
    // Three spaces between them, fewer if a wide font would run out of the box.
    std::string keys;
    for (const char* gap : {"   ", "  ", " "})
    {
        keys = parts.front();
        for (std::size_t i = 1; i < parts.size(); ++i)
            keys += gap + parts[i];
        if (text_.measure(keys) <= box.width - 2 * layout::Padding) break;
    }
    text_.draw(keys, Vector2{box.x + layout::Padding, box.y + box.height - layout::Padding - layout::LineHeight}, data_.ui.dim);
}

void Game::drawTitle()
{
    const std::string title = data_.title;
    const float w = text_.measureScaled(title, 2);
    text_.drawScaled(title, Vector2{std::round(240 - w / 2), 90}, data_.ui.highlight, 2);
    const std::string version = "v" + std::string(hollow::version());
    text_.draw(version, Vector2{std::round(240 - text_.measure(version) / 2), 112}, data_.ui.dim);

    // The one warm thing on a cold screen.
    const float t = static_cast<float>(GetTime());
    const float flicker = 1.0f + 0.08f * std::sin(t * 6.1f) + 0.05f * std::sin(t * 13.7f);
    DrawRectangle(239, 128, 2, 2, data_.ui.accent);
    drawGlow(Vector2{240, 129}, 18.0f * flicker, data_.ui.accent, 0.9f);

    titleMenu_.draw(text_, data_.ui, Vector2{240, 190});
    const char* help = "F11: fullscreen / window";
    text_.draw(help, Vector2{std::round(240 - text_.measure(help) / 2), 252}, data_.ui.dim);

    // Who made it, bottom left (game.json "credits"); the engine, bottom right.
    float y = 262.0f - 10.0f * static_cast<float>(data_.credits.size());
    for (const std::string& line : data_.credits)
    {
        text_.draw(line, Vector2{8, y}, data_.ui.dim);
        y += 10.0f;
    }
    const char* engine = "Powered by raylib";
    text_.draw(engine, Vector2{std::round(472 - text_.measure(engine)), 252}, data_.ui.dim);
}

void Game::drawWarning()
{
    // Over the top of the scene, blinking, under a boss bar if there is one.
    if (warning_ <= 0.0f || std::fmod(warning_, 0.6f) < 0.18f) return;
    const float w = text_.measure(warningText_);
    const Rectangle box{std::round(240 - w / 2 - 6), 28, std::round(w + 12), layout::LineHeight + 6};
    DrawRectangleRec(box, ColorAlpha(BLACK, 0.75f));
    text_.draw(warningText_, Vector2{box.x + 6, box.y + 3}, Color{230, 92, 80, 255});
}

void Game::drawDead()
{
    // The scene, frozen, going dark red; then the way back.
    const float k = std::min(1.0f, deadTime_ / DeadWait);
    DrawRectangle(0, 0, layout::CanvasWidth, layout::CanvasHeight, ColorAlpha(Color{40, 4, 6, 255}, 0.8f * k));
    const float w = text_.measureScaled(deadText_, 2);
    text_.drawScaled(deadText_, Vector2{std::round(240 - w / 2), 104}, ColorAlpha(Color{230, 92, 80, 255}, k), 2);
    if (deadTime_ < DeadWait) return;
    const char* again = "E: try this level again";
    const char* quit = "Esc: quit to title";
    text_.draw(again, Vector2{std::round(240 - text_.measure(again) / 2), 150}, data_.ui.highlight);
    text_.draw(quit, Vector2{std::round(240 - text_.measure(quit) / 2), 164}, data_.ui.dim);
}

void Game::drawPause()
{
    DrawRectangle(0, 0, layout::CanvasWidth, layout::CanvasHeight, ColorAlpha(BLACK, 0.6f));
    const Rectangle panel{180, 100, 120, 70};
    drawPanel(assets_, panel, data_.ui);
    text_.draw("Paused", Vector2{std::round(240 - text_.measure("Paused") / 2), panel.y + 6}, data_.ui.dim);
    pauseMenu_.draw(text_, data_.ui, Vector2{240, 140});
}

void Game::drawSettings()
{
    DrawRectangle(0, 0, layout::CanvasWidth, layout::CanvasHeight, ColorAlpha(BLACK, 0.6f));
    const Rectangle panel{140, 84, 200, 104};
    drawPanel(assets_, panel, data_.ui);
    const char* title = "Settings";
    text_.draw(title, Vector2{std::round(240 - text_.measure(title) / 2), panel.y + 6}, data_.ui.dim);
    settingsMenu_.draw(text_, data_.ui, Vector2{240, 120});
    const char* keys = "E or A/D: next font";
    text_.draw(keys, Vector2{std::round(240 - text_.measure(keys) / 2), panel.y + 58}, data_.ui.dim);

    // A line of the game in the chosen font, to judge it by.
    const char* sample[] = {"Supplies are low. Something is", "following them. 0123456789 ?!"};
    const float y = panel.y + panel.height - 30;
    text_.draw(sample[0], Vector2{panel.x + 10, y}, data_.ui.text);
    text_.draw(sample[1], Vector2{panel.x + 10, y + layout::LineHeight}, data_.ui.thought);
}

void Game::drawEndCard()
{
    float y = 110.0f - static_cast<float>(data_.endCard.size()) * 7.0f;
    for (std::size_t i = 0; i < data_.endCard.size(); ++i)
    {
        const float alpha = std::clamp((endCardTime_ - 1.0f - 2.0f * static_cast<float>(i)) / 2.0f, 0.0f, 1.0f);
        const std::string& line = data_.endCard[i];
        const Color color = line == "END" ? data_.ui.dim : data_.ui.text;
        text_.draw(line, Vector2{std::round(240 - text_.measure(line) / 2), y}, ColorAlpha(color, alpha));
        y += line.empty() ? 8.0f : 14.0f;
    }
    if (endCardTime_ > 4.0f && std::fmod(endCardTime_, 1.2f) < 0.8f)
    {
        const char* hint = "Press E";
        text_.draw(hint, Vector2{std::round(240 - text_.measure(hint) / 2), 240}, data_.ui.dim);
    }
}
