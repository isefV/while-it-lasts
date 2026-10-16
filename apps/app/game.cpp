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
    constexpr float BarFlash = 0.8f;       // seconds a bar flashes after it changes

    // Music outside the rooms (assets/sounds/<id>.qoa); rooms name theirs in "music".
    constexpr const char* TitleMusic = "music_title";
    constexpr const char* EndMusic = "music_end";

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

    Rectangle grow(Rectangle r, float by)
    {
        return Rectangle{r.x - by, r.y - by, r.width + 2 * by, r.height + 2 * by};
    }
} // namespace

Game::Game(std::string assetsDir)
    : assets_(std::move(assetsDir))
    , audio_(assets_)
{
    titleMenu_.items = {"Begin", "Quit"};
    pauseMenu_.items = {"Resume", "Quit to title", "Quit game"};
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
    scene_.load(static_cast<int>(layout::SceneBox.width), static_cast<int>(layout::SceneBox.height));
    text_.load(assets_, data_.ui);
    audio_.init();

    for (const auto& [id, character] : data_.characters)
    {
        Diagnostics diag;
        if (auto sheet = loadSpriteSheet(assets_, character.sprite, diag)) sheets_[id] = std::move(*sheet);
        for (const auto& e : diag.errors)
            TraceLog(LOG_WARNING, "%s", e.c_str());
    }
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
    enterRoom(room.empty() ? data_.startRoom : room, -1.0f);
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
    fade_.set(0.0f);
    return true;
}

bool Game::saveCanvas(const std::string& path) const
{
    Image image = LoadImageFromTexture(canvas_.texture());
    ImageFlipVertical(&image);
    ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8); // as on screen: over black, no see-through pixels
    const bool saved = ExportImage(image, path.c_str());
    UnloadImage(image);
    return saved;
}

void Game::enterRoom(const std::string& id, float spawnX)
{
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
    return dialogueActive() || !pendingRoom_.empty() || inventoryPanel_.isOpen() || ending_ || revive_.active();
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
        if (choice == 0) newGame();
        else if (choice == 1 || input_.pressed(Action::Pause)) quit_ = true;
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
        if (choice == 0)
        {
            screen_ = Screen::Playing;
        }
        else if (choice == 1)
        {
            runner_->stop();
            script_.reset();
            audio_.setAmbient({});
            toTitle();
        }
        else if (choice == 2)
        {
            quit_ = true;
        }
        break;
    }
    case Screen::Playing: updatePlaying(dt); break;
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
        if (result.advance) runner_->advance();
        else if (result.choice >= 0) runner_->choose(result.choice);
        runner_->update(dt);
    }
    else if (revive_.active())
    {
        revive_.update(*this, dt);
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
        companion_.pose = player_.pose == "kneel" ? "kneel" : "idle";
        break;
    }
    case CompanionMode::Drag:
    {
        // X trails on the side the player is walking away from.
        if (player_.moved()) dragSide_ = -player_.facing;
        const float target = player_.x + static_cast<float>(dragSide_) * DragGap;
        companion_.x = approach(companion_.x, target, 80.0f * dt);
        companion_.facing = -dragSide_;
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
    const bool inGame = screen_ == Screen::Playing || screen_ == Screen::Paused;
    if (inGame) drawScene();

    canvas_.begin();
    ClearBackground(BLACK);
    switch (screen_)
    {
    case Screen::Title: drawTitle(); break;
    case Screen::Playing:
    case Screen::Paused:
    {
        const Vector2 offset = shake_.offset();
        scene_.draw(Vector2{layout::SceneBox.x + offset.x, layout::SceneBox.y + offset.y});
        drawHud();
        fade_.draw(Rectangle{0, 0, static_cast<float>(layout::CanvasWidth), static_cast<float>(layout::CanvasHeight)});
        if (screen_ == Screen::Paused) drawPause();
        break;
    }
    case Screen::EndCard: drawEndCard(); break;
    }
    canvas_.end();

    BeginDrawing();
    canvas_.present();
    EndDrawing();
}

void Game::drawScene()
{
    scene_.begin();
    ClearBackground(BLACK);

    if (const Texture2D* background = assets_.texture(room_->background))
    {
        // See-through parts of the art (a window) show the room's colour.
        ClearBackground(multiply(room_->color, tint_));
        DrawTexture(*background, 0, 0, tint_);
    }
    else
    {
        // Placeholder: the room's colour, a darker band below the ground line.
        DrawRectangle(0, 0, 320, 180, multiply(room_->color, tint_));
        DrawRectangle(0, static_cast<int>(layout::GroundY), 320, 30,
                      multiply(mix(room_->color, BLACK, 0.45f), tint_));
    }

    if (script_) script_->drawBack(*this);
    reaper_.draw(*this);

    // Only what the player stands next to is marked, and only that can be used.
    Target highlight;
    std::size_t inReach = 0;
    if (!busy() && !fight_.active())
    {
        highlight = selectedTarget();
        inReach = targetsInReach().size();
    }

    for (const auto& object : room_->objects)
        if (!object.front && !object.hidden && present(object))
            drawObject(object, highlight.object == &object);

    fight_.drawBack(*this);
    companion_.draw(tint_);
    if (highlight.companion) DrawRectangleLinesEx(grow(companion_.bounds(), 1), 1.0f, ColorAlpha(data_.ui.highlight, 0.7f));
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

    if (const BarkLine* line = barks_.current())
    {
        Actor* who = actor(line->speaker);
        const auto it = data_.characters.find(line->speaker);
        if (who && it != data_.characters.end())
            barks_.draw(text_, Vector2{who->x, who->bounds().y}, it->second.textColor);
    }

    if (highlight.valid())
    {
        const Rectangle r = targetRect(highlight);
        const std::string& label = highlight.companion ? companion_.def->name : highlight.object->name;
        std::string note;
        if (inReach > 1)
        {
            const std::vector<Target> targets = targetsInReach();
            const auto index = std::find(targets.begin(), targets.end(), highlight) - targets.begin();
            note = std::to_string(index + 1) + "/" + std::to_string(targets.size());
        }
        drawPrompt(text_, label, Vector2{r.x + r.width / 2.0f, r.y}, data_.ui, note);
    }

    scene_.end();
}

const SpriteSheet* Game::objectSheet(const ObjectDef& object) const
{
    const auto it = objectSheets_.find(object.sprite);
    return it == objectSheets_.end() ? nullptr : &it->second;
}

void Game::drawObject(const ObjectDef& object, bool highlighted)
{
    // An animated sprite: its "idle" frames, top-left at the object's.
    if (const SpriteSheet* sheet = objectSheet(object))
    {
        const auto animation = sheet->animations.find("idle");
        if (animation != sheet->animations.end() &&
            drawSprite(*sheet, objectAnimators_[objectKey(object)],
                       Vector2{object.rect.x + animation->second.origin.x, object.rect.y + animation->second.origin.y}, false, tint_))
        {
            if (highlighted) DrawRectangleLinesEx(grow(object.rect, 1), 1.0f, ColorAlpha(data_.ui.highlight, 0.7f));
            return;
        }
    }
    if (const Texture2D* texture = assets_.texture(object.sprite))
    {
        const Texture2D* hover = highlighted ? assets_.texture(hoverSprite(object.sprite)) : nullptr;
        DrawTexture(hover ? *hover : *texture, static_cast<int>(object.rect.x), static_cast<int>(object.rect.y), tint_);
        if (highlighted && !hover)
            DrawRectangleLinesEx(grow(object.rect, 1), 1.0f, ColorAlpha(data_.ui.highlight, 0.7f));
        return;
    }
    DrawRectangleRec(object.rect, multiply(object.color, tint_));
    if (highlighted) DrawRectangleLinesEx(grow(object.rect, 1), 1.0f, ColorAlpha(data_.ui.highlight, 0.7f));
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
        // "Y [=====   ]": the label, then the bar; it flashes when it changes.
        const BarDef& bar = data_.bars[i];
        const Rectangle slot = layout::BarSlots[i];
        const float labelW = bar.label.empty() ? 0.0f : 10.0f;
        Color color = bar.color;
        if (barFlash_[i] > 0.0f && std::fmod(barFlash_[i], 0.2f) < 0.1f)
            color = barDown_[i] ? Color{170, 60, 60, 255} : data_.ui.highlight;
        if (!bar.label.empty()) text_.draw(bar.label, Vector2{slot.x, slot.y - 1}, data_.ui.dim);
        drawBar(assets_, Rectangle{slot.x + labelW, slot.y, slot.width - labelW, slot.height}, state_.get(bar.var), bar.max,
                color, data_.ui);
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
    std::string keys = "A/D walk   E use";
    if (!busy() && targetsInReach().size() > 1) keys += "   W/S next";
    if (companionMode_ == CompanionMode::Drag) keys += "   G let go";
    else if (!busy() && canGrab()) keys += "   G take hold";
    keys += "   Tab items   Esc pause";
    text_.draw(keys, Vector2{box.x + layout::Padding, box.y + box.height - layout::Padding - layout::LineHeight}, data_.ui.dim);
}

void Game::drawTitle()
{
    const std::string title = data_.title;
    const float w = text_.measureScaled(title, 2);
    text_.drawScaled(title, Vector2{std::round(240 - w / 2), 90}, data_.ui.highlight, 2);

    // The one warm thing on a cold screen.
    const float t = static_cast<float>(GetTime());
    const float flicker = 1.0f + 0.08f * std::sin(t * 6.1f) + 0.05f * std::sin(t * 13.7f);
    DrawRectangle(239, 128, 2, 2, data_.ui.accent);
    drawGlow(Vector2{240, 129}, 18.0f * flicker, data_.ui.accent, 0.9f);

    titleMenu_.draw(text_, data_.ui, Vector2{240, 190});
    const char* help = "F11: fullscreen / window";
    text_.draw(help, Vector2{std::round(240 - text_.measure(help) / 2), 252}, data_.ui.dim);
}

void Game::drawPause()
{
    DrawRectangle(0, 0, layout::CanvasWidth, layout::CanvasHeight, ColorAlpha(BLACK, 0.6f));
    const Rectangle panel{180, 100, 120, 70};
    drawPanel(assets_, panel, data_.ui);
    text_.draw("Paused", Vector2{std::round(240 - text_.measure("Paused") / 2), panel.y + 6}, data_.ui.dim);
    pauseMenu_.draw(text_, data_.ui, Vector2{240, 140});
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
