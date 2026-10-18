#include <hollow/data/game_data.hpp>

#include "../utility/json_util.hpp"

#include <algorithm>
#include <array>
#include <filesystem>

namespace hollow
{
    using detail::child;
    using detail::Json;

    TokenMap GameData::tokens() const
    {
        TokenMap map;
        for (const auto& [id, character] : characters)
            map[id] = character.name;
        return map;
    }

    namespace
    {
        // ---------------------------------------------------------------------
        // Pieces shared by every file
        // ---------------------------------------------------------------------

        Condition readCondition(const Json& obj, std::string_view key, const std::string& where, Diagnostics& diag)
        {
            const std::string text = detail::readString(obj, key, where, diag);
            std::string error;
            auto condition = Condition::parse(text, error);
            if (!condition)
            {
                diag.error(child(where, key) + ": \"" + text + "\": " + error);
                return {};
            }
            return *condition;
        }

        std::vector<Effect> readEffects(const Json& obj, std::string_view key, const std::string& where,
                                        Diagnostics& diag)
        {
            std::vector<Effect> effects;
            const auto texts = detail::readStrings(obj, key, where, diag);
            for (std::size_t i = 0; i < texts.size(); ++i)
            {
                std::string error;
                if (auto effect = Effect::parse(texts[i], error))
                    effects.push_back(std::move(*effect));
                else
                    diag.error(child(child(where, key), i) + ": \"" + texts[i] + "\": " + error);
            }
            return effects;
        }

        bool requireObject(const Json& value, const std::string& where, Diagnostics& diag)
        {
            if (value.is_object()) return true;
            diag.error(where + ": expected an object { ... }, found " + detail::typeName(value));
            return false;
        }

        // ---------------------------------------------------------------------
        // Dialogue
        // ---------------------------------------------------------------------

        constexpr std::array<std::string_view, 11> ReservedEntryKeys = {
            "if", "do", "then", "else", "choice", "timer", "goto", "pause", "end", "thought", "narration"};

        bool isReserved(std::string_view key)
        {
            return std::find(ReservedEntryKeys.begin(), ReservedEntryKeys.end(), key) != ReservedEntryKeys.end();
        }

        std::vector<DialogueEntry> parseEntries(const Json& list, const std::string& where, Diagnostics& diag);

        DialogueOption parseOption(const Json& obj, const std::string& where, Diagnostics& diag)
        {
            DialogueOption option;
            if (!requireObject(obj, where, diag)) return option;
            detail::checkKeys(obj, {"text", "if", "do", "next", "timeout"}, where, diag);

            option.text = detail::readString(obj, "text", where, diag);
            option.condition = readCondition(obj, "if", where, diag);
            option.effects = readEffects(obj, "do", where, diag);
            option.next = detail::readString(obj, "next", where, diag);
            option.timeout = detail::readBool(obj, "timeout", where, diag);
            if (option.text.empty() && !option.timeout)
                diag.error(where + ": an option needs \"text\" (only a \"timeout\" option may have none)");
            return option;
        }

        DialogueEntry parseEntry(const Json& obj, const std::string& where, Diagnostics& diag)
        {
            DialogueEntry entry;
            entry.where = where;
            if (!requireObject(obj, where, diag)) return entry;

            entry.condition = readCondition(obj, "if", where, diag);
            entry.effects = readEffects(obj, "do", where, diag);

            std::vector<std::string> speakers;
            for (const auto& [key, value] : obj.items())
                if (!key.empty() && key[0] != '_' && !isReserved(key)) speakers.push_back(key);

            // What kind of entry: exactly one of these keys decides it.
            std::vector<std::string> kinds;
            for (const char* key : {"choice", "goto", "pause", "then", "thought", "narration", "end"})
                if (obj.contains(key)) kinds.emplace_back(key);
            if (!speakers.empty()) kinds.push_back("\"" + speakers.front() + "\" (a speaker)");

            if (speakers.size() > 1)
            {
                diag.error(where + ": one line has one speaker; found \"" + speakers[0] + "\" and \"" + speakers[1] +
                           "\" (or a misspelled key)");
                return entry;
            }
            if (kinds.size() > 1)
            {
                diag.error(where + ": an entry is one thing; it has both " + kinds[0] + " and " + kinds[1] +
                           ". Split it into two entries.");
                return entry;
            }
            if (obj.contains("else") && !obj.contains("then"))
                diag.error(where + ": \"else\" needs \"then\"");
            if (obj.contains("timer") && !obj.contains("choice"))
                diag.error(where + ": \"timer\" belongs to a \"choice\"");

            if (obj.contains("choice"))
            {
                entry.kind = DialogueEntry::Kind::Choice;
                entry.timer = detail::readNumber(obj, "timer", where, diag);
                const Json& options = obj["choice"];
                const std::string at = child(where, "choice");
                if (!options.is_array() || options.empty())
                {
                    diag.error(at + ": expected a list of options [ { \"text\": ... }, ... ]");
                    return entry;
                }
                for (std::size_t i = 0; i < options.size(); ++i)
                    entry.options.push_back(parseOption(options[i], child(at, i), diag));
            }
            else if (obj.contains("goto"))
            {
                entry.kind = DialogueEntry::Kind::Goto;
                entry.target = detail::readString(obj, "goto", where, diag);
            }
            else if (obj.contains("pause"))
            {
                entry.kind = DialogueEntry::Kind::Pause;
                entry.seconds = detail::readNumber(obj, "pause", where, diag, 1.0f);
            }
            else if (obj.contains("then"))
            {
                entry.kind = DialogueEntry::Kind::Block;
                if (entry.condition.empty())
                    diag.error(where + ": \"then\" needs an \"if\"");
                entry.thenEntries = parseEntries(obj["then"], child(where, "then"), diag);
                if (obj.contains("else"))
                    entry.elseEntries = parseEntries(obj["else"], child(where, "else"), diag);
            }
            else if (obj.contains("thought") || obj.contains("narration"))
            {
                const bool thought = obj.contains("thought");
                entry.kind = DialogueEntry::Kind::Line;
                entry.style = thought ? LineStyle::Thought : LineStyle::Narration;
                entry.text = detail::readString(obj, thought ? "thought" : "narration", where, diag);
            }
            else if (!speakers.empty())
            {
                entry.kind = DialogueEntry::Kind::Line;
                entry.style = LineStyle::Speech;
                entry.speaker = speakers.front();
                entry.text = detail::readString(obj, entry.speaker, where, diag);
            }
            else if (obj.contains("end"))
            {
                entry.kind = DialogueEntry::Kind::End;
            }
            else if (obj.contains("do"))
            {
                entry.kind = DialogueEntry::Kind::Action;
            }
            else
            {
                diag.error(where + ": the entry has no line, choice, goto, pause, then or do");
            }
            return entry;
        }

        std::vector<DialogueEntry> parseEntries(const Json& list, const std::string& where, Diagnostics& diag)
        {
            std::vector<DialogueEntry> entries;
            if (!list.is_array())
            {
                diag.error(where + ": expected a list of entries [ ... ], found " + detail::typeName(list));
                return entries;
            }
            for (std::size_t i = 0; i < list.size(); ++i)
                entries.push_back(parseEntry(list[i], child(where, i), diag));
            return entries;
        }

        void parseDialogue(const Json& root, const std::string& file, ConversationMap& out, Diagnostics& diag)
        {
            if (!requireObject(root, file, diag)) return;
            for (const auto& [id, list] : root.items())
            {
                if (!id.empty() && id[0] == '_') continue;
                const std::string where = file + ": " + id;
                if (out.contains(id))
                {
                    diag.error(where + ": conversation \"" + id + "\" is already defined in " + out.at(id).file);
                    continue;
                }
                Conversation conversation;
                conversation.id = id;
                conversation.file = file;
                conversation.entries = parseEntries(list, where, diag);
                out.emplace(id, std::move(conversation));
            }
        }

        // ---------------------------------------------------------------------
        // Rooms
        // ---------------------------------------------------------------------

        void readRange(const Json& obj, std::string_view key, const std::string& where, Diagnostics& diag, float& lo,
                       float& hi)
        {
            if (!obj.contains(key)) return;
            const Json& value = obj[std::string(key)];
            if (value.is_array() && value.size() == 2 && value[0].is_number() && value[1].is_number())
            {
                lo = value[0].get<float>();
                hi = value[1].get<float>();
                return;
            }
            diag.error(child(where, key) + ": expected [from, to], two numbers");
        }

        ObjectDef parseObject(const Json& obj, const std::string& where, Diagnostics& diag)
        {
            ObjectDef def;
            def.where = where;
            if (!requireObject(obj, where, diag)) return def;
            detail::checkKeys(obj,
                              {"id", "name", "x", "y", "w", "h", "color", "sprite", "scale", "front", "behind", "alpha", "hidden", "if",
                               "dialogue", "again", "interactive", "once", "exit", "spawn_x", "exit_if", "locked",
                               "trigger", "repeat", "cover", "rest", "block", "blocked", "effect", "event"},
                              where, diag);

            def.id = detail::readString(obj, "id", where, diag);
            if (def.id.empty()) diag.error(where + ": every object needs an \"id\"");
            def.name = detail::readString(obj, "name", where, diag);

            def.rect.width = detail::readNumber(obj, "w", where, diag, 16.0f);
            def.rect.height = detail::readNumber(obj, "h", where, diag, 16.0f);
            def.rect.x = detail::readNumber(obj, "x", where, diag);
            // Without "y" the object stands on the ground line.
            def.rect.y = detail::readNumber(obj, "y", where, diag, 150.0f - def.rect.height);

            def.color = detail::readColor(obj, "color", where, diag, def.color);
            def.sprite = detail::readString(obj, "sprite", where, diag);
            def.scale = detail::readNumber(obj, "scale", where, diag, 1.0f);
            def.front = detail::readBool(obj, "front", where, diag);
            def.behind = detail::readBool(obj, "behind", where, diag);
            def.alpha = std::clamp(detail::readNumber(obj, "alpha", where, diag, 1.0f), 0.0f, 1.0f);
            def.hidden = detail::readBool(obj, "hidden", where, diag);
            def.condition = readCondition(obj, "if", where, diag);

            def.dialogue = detail::readString(obj, "dialogue", where, diag);
            def.again = detail::readString(obj, "again", where, diag);
            def.once = detail::readBool(obj, "once", where, diag);
            def.exitTo = detail::readString(obj, "exit", where, diag);
            def.spawnX = detail::readNumber(obj, "spawn_x", where, diag, -1.0f);
            def.exitCondition = readCondition(obj, "exit_if", where, diag);
            def.locked = detail::readString(obj, "locked", where, diag);
            def.trigger = detail::readBool(obj, "trigger", where, diag);
            def.repeat = detail::readBool(obj, "repeat", where, diag);
            def.cover = detail::readBool(obj, "cover", where, diag);
            def.rest = detail::readBool(obj, "rest", where, diag);
            def.hasBlock = obj.contains("block");
            def.block = readCondition(obj, "block", where, diag);
            def.blocked = detail::readString(obj, "blocked", where, diag);
            def.effect = detail::readString(obj, "effect", where, diag);
            def.event = detail::readString(obj, "event", where, diag);

            const bool doesSomething = !def.dialogue.empty() || !def.exitTo.empty() || !def.event.empty();
            def.interactive = detail::readBool(obj, "interactive", where, diag, doesSomething && !def.trigger);
            if (def.name.empty()) def.name = def.id;
            if (def.trigger) def.hidden = obj.contains("hidden") ? def.hidden : true;
            return def;
        }

        RoomDef parseRoom(const std::string& id, const Json& obj, const std::string& file, Diagnostics& diag)
        {
            RoomDef room;
            room.id = id;
            room.file = file;
            const std::string where = file + ": " + id;
            if (!requireObject(obj, where, diag)) return room;
            detail::checkKeys(obj,
                              {"name", "background", "frame", "flip", "color", "tint", "fog", "ambient", "steps", "music", "player_x", "walk",
                               "companion", "creature", "fight", "enter", "objects", "idle_barks", "script"},
                              where, diag);

            room.name = detail::readString(obj, "name", where, diag, id);
            room.background = detail::readString(obj, "background", where, diag);
            room.color = detail::readColor(obj, "color", where, diag, room.color);
            room.tint = detail::readColor(obj, "tint", where, diag, room.tint);
            room.fog = std::clamp(detail::readNumber(obj, "fog", where, diag, 0.0f), 0.0f, 1.0f);
            room.frame = detail::readBool(obj, "frame", where, diag);
            room.flip = detail::readBool(obj, "flip", where, diag);
            room.ambient = detail::readStrings(obj, "ambient", where, diag);
            room.steps = detail::readStrings(obj, "steps", where, diag);
            room.music = detail::readString(obj, "music", where, diag);
            room.playerX = detail::readNumber(obj, "player_x", where, diag, room.playerX);
            readRange(obj, "walk", where, diag, room.walkMin, room.walkMax);
            room.idleBarks = detail::readStrings(obj, "idle_barks", where, diag);
            room.script = detail::readString(obj, "script", where, diag);

            if (obj.contains("companion"))
            {
                const Json& c = obj["companion"];
                const std::string at = child(where, "companion");
                if (requireObject(c, at, diag))
                {
                    detail::checkKeys(c, {"x", "mode", "pose", "dialogue", "again", "bark", "release", "grab"}, at, diag);
                    room.companion.present = true;
                    room.companion.x = detail::readNumber(c, "x", at, diag, room.playerX - 20.0f);
                    room.companion.mode = detail::readString(c, "mode", at, diag, "stay");
                    room.companion.pose = detail::readString(c, "pose", at, diag, "idle");
                    room.companion.dialogue = detail::readString(c, "dialogue", at, diag);
                    room.companion.again = detail::readString(c, "again", at, diag);
                    room.companion.bark = detail::readString(c, "bark", at, diag);
                    room.companion.release = detail::readString(c, "release", at, diag);
                    room.companion.grab = detail::readString(c, "grab", at, diag);
                    const auto& mode = room.companion.mode;
                    if (mode != "stay" && mode != "follow" && mode != "drag")
                        diag.error(child(at, "mode") + ": \"" + mode + "\" is not stay, follow or drag");
                }
            }

            if (obj.contains("creature"))
            {
                const Json& c = obj["creature"];
                const std::string at = child(where, "creature");
                if (requireObject(c, at, diag))
                {
                    detail::checkKeys(c, {"x", "y", "w", "h", "speed", "color", "sprite", "scale", "patrol", "phases", "if"},
                                      at, diag);
                    CreatureDef& creature = room.creature;
                    creature.present = true;
                    creature.condition = readCondition(c, "if", at, diag);
                    creature.x = detail::readNumber(c, "x", at, diag, 200.0f);
                    creature.y = detail::readNumber(c, "y", at, diag, 150.0f);
                    creature.width = detail::readNumber(c, "w", at, diag, creature.width);
                    creature.height = detail::readNumber(c, "h", at, diag, creature.height);
                    creature.speed = detail::readNumber(c, "speed", at, diag, creature.speed);
                    creature.color = detail::readColor(c, "color", at, diag, creature.color);
                    creature.sprite = detail::readString(c, "sprite", at, diag);
                    creature.scale = detail::readNumber(c, "scale", at, diag, 1.0f);
                    creature.minX = creature.x;
                    creature.maxX = creature.x;
                    readRange(c, "patrol", at, diag, creature.minX, creature.maxX);

                    if (c.contains("phases"))
                    {
                        const Json& phases = c["phases"];
                        const std::string pat = child(at, "phases");
                        if (!phases.is_array()) diag.error(pat + ": expected a list of phases");
                        for (std::size_t i = 0; phases.is_array() && i < phases.size(); ++i)
                        {
                            const std::string ph = child(pat, i);
                            if (!requireObject(phases[i], ph, diag)) continue;
                            detail::checkKeys(phases[i], {"state", "time", "to"}, ph, diag);
                            PatrolPhase phase;
                            phase.state = detail::readString(phases[i], "state", ph, diag);
                            phase.time = detail::readNumber(phases[i], "time", ph, diag, 1.0f);
                            phase.toX = detail::readNumber(phases[i], "to", ph, diag, -1.0f);
                            if (phase.time <= 0.0f) diag.error(child(ph, "time") + ": must be above 0");
                            creature.phases.push_back(std::move(phase));
                        }
                    }
                }
            }

            if (obj.contains("fight"))
            {
                const Json& f = obj["fight"];
                const std::string at = child(where, "fight");
                if (requireObject(f, at, diag))
                {
                    detail::checkKeys(f, {"brute", "hp", "damage", "waves", "win"}, at, diag);
                    FightDef& fight = room.fight;
                    fight.present = true;
                    fight.brute = detail::readBool(f, "brute", at, diag);
                    fight.hp = std::max(1, detail::readInt(f, "hp", at, diag, fight.hp));
                    fight.damage = std::max(1, detail::readInt(f, "damage", at, diag, fight.damage));
                    fight.win = detail::readString(f, "win", at, diag);
                    if (fight.brute && !room.creature.present)
                        diag.error(child(at, "brute") + ": the room has no \"creature\" to fight");
                    if (f.contains("waves"))
                    {
                        const Json& waves = f["waves"];
                        const std::string wat = child(at, "waves");
                        if (!waves.is_array()) diag.error(wat + ": expected a list of waves");
                        for (std::size_t i = 0; waves.is_array() && i < waves.size(); ++i)
                        {
                            const std::string w = child(wat, i);
                            if (!requireObject(waves[i], w, diag)) continue;
                            detail::checkKeys(waves[i], {"crawlers", "from"}, w, diag);
                            WaveDef wave;
                            wave.crawlers = std::max(1, detail::readInt(waves[i], "crawlers", w, diag, wave.crawlers));
                            wave.from = detail::readString(waves[i], "from", w, diag, wave.from);
                            if (wave.from != "right" && wave.from != "left" && wave.from != "both")
                                diag.error(child(w, "from") + ": \"" + wave.from + "\" is not right, left or both");
                            fight.waves.push_back(std::move(wave));
                        }
                    }
                    if (!fight.brute && fight.waves.empty())
                        diag.error(at + ": a fight needs \"brute\": true or at least one wave");
                }
            }

            if (obj.contains("enter"))
            {
                const Json& rules = obj["enter"];
                const std::string at = child(where, "enter");
                if (!rules.is_array()) diag.error(at + ": expected a list [ { \"dialogue\": ... }, ... ]");
                for (std::size_t i = 0; rules.is_array() && i < rules.size(); ++i)
                {
                    const std::string rat = child(at, i);
                    if (!requireObject(rules[i], rat, diag)) continue;
                    detail::checkKeys(rules[i], {"if", "dialogue", "do", "once"}, rat, diag);
                    EnterRule rule;
                    rule.where = rat;
                    rule.condition = readCondition(rules[i], "if", rat, diag);
                    rule.dialogue = detail::readString(rules[i], "dialogue", rat, diag);
                    rule.effects = readEffects(rules[i], "do", rat, diag);
                    rule.once = detail::readBool(rules[i], "once", rat, diag, true);
                    room.onEnter.push_back(std::move(rule));
                }
            }

            if (obj.contains("objects"))
            {
                const Json& objects = obj["objects"];
                const std::string at = child(where, "objects");
                if (!objects.is_array()) diag.error(at + ": expected a list of objects");
                for (std::size_t i = 0; objects.is_array() && i < objects.size(); ++i)
                {
                    ObjectDef def = parseObject(objects[i], child(at, i), diag);
                    const bool duplicate = std::any_of(room.objects.begin(), room.objects.end(),
                                                       [&](const ObjectDef& o) { return o.id == def.id; });
                    if (duplicate && !def.id.empty())
                        diag.error(child(at, i) + ": another object in this room is already called \"" + def.id + "\"");
                    room.objects.push_back(std::move(def));
                }
            }
            return room;
        }

        // ---------------------------------------------------------------------
        // Items, barks, game.json
        // ---------------------------------------------------------------------

        void parseItems(const Json& root, const std::string& file, GameData& data, Diagnostics& diag)
        {
            if (!requireObject(root, file, diag)) return;
            for (const auto& [id, obj] : root.items())
            {
                if (!id.empty() && id[0] == '_') continue;
                const std::string where = file + ": " + id;
                if (!requireObject(obj, where, diag)) continue;
                detail::checkKeys(obj, {"name", "description", "icon", "color", "count", "use"}, where, diag);

                ItemDef item;
                item.id = id;
                item.name = detail::readString(obj, "name", where, diag, id);
                item.description = detail::readString(obj, "description", where, diag);
                item.icon = detail::readString(obj, "icon", where, diag);
                item.color = detail::readColor(obj, "color", where, diag, item.color);
                item.countVar = detail::readString(obj, "count", where, diag);
                item.use = detail::readString(obj, "use", where, diag);
                data.itemOrder.push_back(id);
                data.items[id] = std::move(item);
            }
        }

        BarkLine parseBarkLine(const Json& obj, const std::string& where, Diagnostics& diag)
        {
            BarkLine line;
            if (!requireObject(obj, where, diag)) return line;
            int count = 0;
            for (const auto& [key, value] : obj.items())
            {
                if (!key.empty() && key[0] == '_') continue;
                ++count;
                line.speaker = key;
                line.text = value.is_string() ? value.get<std::string>() : "";
                if (!value.is_string()) diag.error(child(where, key) + ": expected the line as a string");
            }
            if (count != 1) diag.error(where + ": a bark line is { \"speaker\": \"text\" }, one speaker");
            return line;
        }

        void parseBarks(const Json& root, const std::string& file, GameData& data, Diagnostics& diag)
        {
            if (!requireObject(root, file, diag)) return;
            for (const auto& [id, obj] : root.items())
            {
                if (!id.empty() && id[0] == '_') continue;
                const std::string where = file + ": " + id;
                if (!requireObject(obj, where, diag)) continue;
                detail::checkKeys(obj, {"if", "lines"}, where, diag);

                BarkPool pool;
                pool.id = id;
                pool.where = where;
                pool.condition = readCondition(obj, "if", where, diag);
                const Json& lines = obj.contains("lines") ? obj["lines"] : Json::array();
                const std::string at = child(where, "lines");
                if (!lines.is_array() || lines.empty()) diag.error(at + ": expected a list of lines");
                for (std::size_t i = 0; lines.is_array() && i < lines.size(); ++i)
                {
                    std::vector<BarkLine> entry;
                    if (lines[i].is_array())
                    {
                        for (std::size_t j = 0; j < lines[i].size(); ++j)
                            entry.push_back(parseBarkLine(lines[i][j], child(child(at, i), j), diag));
                    }
                    else
                    {
                        entry.push_back(parseBarkLine(lines[i], child(at, i), diag));
                    }
                    pool.entries.push_back(std::move(entry));
                }
                data.barks[id] = std::move(pool);
            }
        }

        void parseGame(const Json& root, const std::string& file, GameData& data, Diagnostics& diag)
        {
            if (!requireObject(root, file, diag)) return;
            const std::string where = file;
            detail::checkKeys(root,
                              {"title", "start_room", "start_items", "player", "companion", "files", "characters",
                               "variables", "ui", "bars", "end_card", "credits"},
                              where, diag);

            data.title = detail::readString(root, "title", where, diag, "Hollow");
            data.startRoom = detail::readString(root, "start_room", where, diag);
            data.startItems = detail::readStrings(root, "start_items", where, diag);
            data.player = detail::readString(root, "player", where, diag, data.player);
            data.companion = detail::readString(root, "companion", where, diag, data.companion);
            data.endCard = detail::readStrings(root, "end_card", where, diag);
            data.credits = detail::readStrings(root, "credits", where, diag);

            if (root.contains("characters") && requireObject(root["characters"], child(where, "characters"), diag))
            {
                for (const auto& [id, obj] : root["characters"].items())
                {
                    if (!id.empty() && id[0] == '_') continue;
                    const std::string at = child(child(where, "characters"), id);
                    if (!requireObject(obj, at, diag)) continue;
                    detail::checkKeys(obj, {"name", "color", "text_color", "sprite", "w", "h", "speed"}, at, diag);
                    if (isReserved(id))
                        diag.error(at + ": \"" + id + "\" is a word of the dialogue format; choose another id");

                    CharacterDef c;
                    c.id = id;
                    c.name = detail::readString(obj, "name", at, diag, id);
                    c.color = detail::readColor(obj, "color", at, diag, c.color);
                    c.textColor = detail::readColor(obj, "text_color", at, diag, c.textColor);
                    c.sprite = detail::readString(obj, "sprite", at, diag);
                    c.width = detail::readNumber(obj, "w", at, diag, c.width);
                    c.height = detail::readNumber(obj, "h", at, diag, c.height);
                    c.speed = detail::readNumber(obj, "speed", at, diag, c.speed);
                    data.characters[id] = std::move(c);
                }
            }

            if (root.contains("variables") && requireObject(root["variables"], child(where, "variables"), diag))
            {
                for (const auto& [name, value] : root["variables"].items())
                {
                    if (!name.empty() && name[0] == '_') continue;
                    const std::string at = child(child(where, "variables"), name);
                    VariableDef v;
                    v.name = name;
                    if (value.is_boolean())
                    {
                        v.value = value.get<bool>() ? 1 : 0;
                    }
                    else if (value.is_number_integer())
                    {
                        v.value = value.get<int>();
                        v.max = std::max(99, v.value);
                    }
                    else if (value.is_object())
                    {
                        detail::checkKeys(value, {"value", "min", "max"}, at, diag);
                        v.value = detail::readInt(value, "value", at, diag);
                        v.min = detail::readInt(value, "min", at, diag, 0);
                        v.max = detail::readInt(value, "max", at, diag, std::max(99, v.value));
                    }
                    else
                    {
                        diag.error(at + ": expected true/false, a whole number, or { \"value\", \"min\", \"max\" }");
                        continue;
                    }
                    data.variables.push_back(v);
                }
            }

            if (root.contains("ui") && requireObject(root["ui"], child(where, "ui"), diag))
            {
                const Json& ui = root["ui"];
                const std::string at = child(where, "ui");
                detail::checkKeys(ui,
                                  {"text_speed", "font", "font_size", "fonts", "text", "thought", "narration", "dim",
                                   "box", "frame", "highlight", "accent"},
                                  at, diag);
                UiConfig& c = data.ui;
                c.textSpeed = detail::readNumber(ui, "text_speed", at, diag, c.textSpeed);
                if (ui.contains("font")) c.font = detail::readString(ui, "font", at, diag); // "" = the built-in font
                c.fontSize = detail::readInt(ui, "font_size", at, diag, c.fontSize);
                if (ui.contains("fonts"))
                {
                    const Json& fonts = ui["fonts"];
                    const std::string fat = child(at, "fonts");
                    if (!fonts.is_array()) diag.error(fat + ": expected a list of { \"name\", \"file\", \"size\" }");
                    for (std::size_t i = 0; fonts.is_array() && i < fonts.size(); ++i)
                    {
                        const std::string iat = child(fat, i);
                        if (!requireObject(fonts[i], iat, diag)) continue;
                        detail::checkKeys(fonts[i], {"name", "file", "size"}, iat, diag);
                        FontChoice f;
                        f.name = detail::readString(fonts[i], "name", iat, diag);
                        f.file = detail::readString(fonts[i], "file", iat, diag);
                        f.size = detail::readInt(fonts[i], "size", iat, diag, f.size);
                        if (!f.file.empty() && !std::filesystem::exists(data.assetsDir + f.file))
                            diag.warning(iat + ".file: no such file \"" + f.file + "\"");
                        c.fonts.push_back(f);
                    }
                }
                c.text = detail::readColor(ui, "text", at, diag, c.text);
                c.thought = detail::readColor(ui, "thought", at, diag, c.thought);
                c.narration = detail::readColor(ui, "narration", at, diag, c.narration);
                c.dim = detail::readColor(ui, "dim", at, diag, c.dim);
                c.box = detail::readColor(ui, "box", at, diag, c.box);
                c.frame = detail::readColor(ui, "frame", at, diag, c.frame);
                c.highlight = detail::readColor(ui, "highlight", at, diag, c.highlight);
                c.accent = detail::readColor(ui, "accent", at, diag, c.accent);
            }

            if (root.contains("bars"))
            {
                const Json& bars = root["bars"];
                const std::string at = child(where, "bars");
                if (!bars.is_array()) diag.error(at + ": expected a list of bars");
                for (std::size_t i = 0; bars.is_array() && i < bars.size(); ++i)
                {
                    const std::string bat = child(at, i);
                    if (!requireObject(bars[i], bat, diag)) continue;
                    detail::checkKeys(bars[i], {"label", "var", "max", "color"}, bat, diag);
                    BarDef bar;
                    bar.label = detail::readString(bars[i], "label", bat, diag);
                    bar.var = detail::readString(bars[i], "var", bat, diag);
                    bar.max = std::max(1, detail::readInt(bars[i], "max", bat, diag, 1));
                    bar.color = detail::readColor(bars[i], "color", bat, diag, bar.color);
                    data.bars.push_back(std::move(bar));
                }
                if (data.bars.size() > 3) diag.error(at + ": the screen has room for 3 bars");
            }
        }

        // ---------------------------------------------------------------------
        // Checks across files
        // ---------------------------------------------------------------------

        class Checker
        {
        public:
            Checker(const GameData& data, const std::set<std::string, std::less<>>& commands, Diagnostics& diag)
                : data_(data)
                , commands_(commands)
                , diag_(diag)
            {
            }

            void run()
            {
                if (data_.startRoom.empty())
                    diag_.error("data/game.json: start_room: missing");
                else
                    room(data_.startRoom, "data/game.json: start_room");
                character(data_.player, "data/game.json: player");
                character(data_.companion, "data/game.json: companion");
                for (const auto& id : data_.startItems)
                    item(id, "data/game.json: start_items");
                for (std::size_t i = 0; i < data_.bars.size(); ++i)
                    variable(data_.bars[i].var, "data/game.json: bars[" + std::to_string(i) + "].var");
                for (const auto& [id, item] : data_.items)
                {
                    if (!item.countVar.empty()) variable(item.countVar, "data/items.json: " + id + ".count");
                    conversation(item.use, "data/items.json: " + id + ".use");
                }

                for (const auto& [id, conversation] : data_.conversations)
                    entries(conversation.entries);
                for (const auto& [id, def] : data_.rooms)
                    roomDef(def);
                for (const auto& [id, pool] : data_.barks)
                {
                    names(pool.condition, pool.where + ".if");
                    for (const auto& entry : pool.entries)
                        for (const auto& line : entry)
                            character(line.speaker, pool.where);
                }
            }

        private:
            void room(const std::string& id, const std::string& where)
            {
                if (!data_.rooms.contains(id)) diag_.error(where + ": unknown room \"" + id + "\"");
            }

            void conversation(const std::string& id, const std::string& where)
            {
                if (!id.empty() && !data_.conversations.contains(id))
                    diag_.error(where + ": unknown conversation \"" + id + "\"");
            }

            void character(const std::string& id, const std::string& where)
            {
                if (!data_.characters.contains(id)) diag_.error(where + ": unknown character \"" + id + "\"");
            }

            void item(const std::string& id, const std::string& where)
            {
                if (!data_.items.contains(id)) diag_.error(where + ": unknown item \"" + id + "\"");
            }

            void bark(const std::string& id, const std::string& where)
            {
                if (!id.empty() && !data_.barks.contains(id)) diag_.error(where + ": unknown bark pool \"" + id + "\"");
            }

            bool declared(std::string_view name) const
            {
                return std::any_of(data_.variables.begin(), data_.variables.end(),
                                   [&](const VariableDef& v) { return v.name == name; });
            }

            void variable(const std::string& name, const std::string& where)
            {
                if (name.starts_with("item."))
                {
                    item(name.substr(5), where);
                    return;
                }
                if (name.starts_with("seen."))
                {
                    conversation(name.substr(5), where);
                    return;
                }
                if (!declared(name))
                    diag_.error(where + ": unknown variable \"" + name + "\" (declare it in data/game.json \"variables\")");
            }

            void names(const Condition& condition, const std::string& where)
            {
                for (const auto& name : condition.names())
                    variable(name, where);
            }

            void effects(const std::vector<Effect>& list, const std::string& where)
            {
                for (const auto& effect : list)
                {
                    const std::string at = where + " \"" + effect.source + "\"";
                    switch (effect.kind)
                    {
                    case Effect::Kind::Set:
                    case Effect::Kind::Add:
                        if (!declared(effect.name))
                            diag_.error(at + ": unknown variable \"" + effect.name + "\"");
                        break;
                    case Effect::Kind::Give:
                    case Effect::Kind::Take: item(effect.name, at); break;
                    case Effect::Kind::Command:
                        if (!commands_.contains(effect.name))
                            diag_.error(at + ": unknown command \"" + effect.name + "\"");
                        else if (effect.name == "goto" && !effect.args.empty())
                            room(effect.args[0], at);
                        else if (effect.name == "bark" && !effect.args.empty())
                            bark(effect.args[0], at);
                        else if ((effect.name == "start" || effect.name == "revive") && !effect.args.empty())
                            conversation(effect.args[0], at);
                        break;
                    }
                }
            }

            void entries(const std::vector<DialogueEntry>& list)
            {
                for (const auto& entry : list)
                {
                    names(entry.condition, entry.where + ".if");
                    effects(entry.effects, entry.where + ".do");
                    switch (entry.kind)
                    {
                    case DialogueEntry::Kind::Line:
                        if (entry.style == LineStyle::Speech) character(entry.speaker, entry.where);
                        if (entry.text.empty()) diag_.error(entry.where + ": the line is empty");
                        if (entry.text.size() > 200)
                            diag_.warning(entry.where + ": long line (" + std::to_string(entry.text.size()) +
                                          " letters); it will need more than one box");
                        break;
                    case DialogueEntry::Kind::Choice:
                    {
                        int shown = 0;
                        int timeouts = 0;
                        for (std::size_t i = 0; i < entry.options.size(); ++i)
                        {
                            const auto& option = entry.options[i];
                            const std::string at = entry.where + ".choice[" + std::to_string(i) + "]";
                            names(option.condition, at + ".if");
                            effects(option.effects, at + ".do");
                            conversation(option.next, at + ".next");
                            if (!option.text.empty()) ++shown;
                            if (option.timeout) ++timeouts;
                        }
                        if (shown > 4)
                            diag_.warning(entry.where + ": " + std::to_string(shown) +
                                          " options; the box shows 4, so make sure their \"if\" leave at most 4");
                        if (entry.timer > 0.0f && timeouts == 0)
                            diag_.error(entry.where + ": a choice with a \"timer\" needs one option with \"timeout\": true");
                        if (entry.timer <= 0.0f && timeouts > 0)
                            diag_.warning(entry.where + ": an option has \"timeout\" but the choice has no \"timer\"");
                        break;
                    }
                    case DialogueEntry::Kind::Goto: conversation(entry.target, entry.where + ".goto"); break;
                    case DialogueEntry::Kind::Block:
                        entries(entry.thenEntries);
                        entries(entry.elseEntries);
                        break;
                    default: break;
                    }
                }
            }

            void roomDef(const RoomDef& def)
            {
                const std::string where = def.file + ": " + def.id;
                for (const auto& rule : def.onEnter)
                {
                    names(rule.condition, rule.where + ".if");
                    effects(rule.effects, rule.where + ".do");
                    conversation(rule.dialogue, rule.where + ".dialogue");
                }
                if (def.companion.present)
                {
                    conversation(def.companion.dialogue, where + ".companion.dialogue");
                    conversation(def.companion.again, where + ".companion.again");
                    bark(def.companion.bark, where + ".companion.bark");
                    conversation(def.companion.release, where + ".companion.release");
                    conversation(def.companion.grab, where + ".companion.grab");
                }
                if (def.creature.present) names(def.creature.condition, where + ".creature.if");
                if (def.fight.present)
                {
                    conversation(def.fight.win, where + ".fight.win");
                }
                for (const auto& id : def.idleBarks)
                    bark(id, where + ".idle_barks");
                for (const auto& object : def.objects)
                {
                    names(object.condition, object.where + ".if");
                    names(object.exitCondition, object.where + ".exit_if");
                    names(object.block, object.where + ".block");
                    conversation(object.dialogue, object.where + ".dialogue");
                    conversation(object.again, object.where + ".again");
                    conversation(object.locked, object.where + ".locked");
                    conversation(object.blocked, object.where + ".blocked");
                    if (!object.exitTo.empty()) room(object.exitTo, object.where + ".exit");
                    if (object.trigger && object.dialogue.empty() && object.event.empty())
                        diag_.warning(object.where + ": a trigger with no dialogue and no event does nothing");
                }
            }

            const GameData& data_;
            const std::set<std::string, std::less<>>& commands_;
            Diagnostics& diag_;
        };
    } // namespace

    void parseDialogueText(std::string_view text, const std::string& file, ConversationMap& out, Diagnostics& diag)
    {
        if (const auto root = detail::parseJson(text, file, diag)) parseDialogue(*root, file, out, diag);
    }

    GameData loadGameData(const std::string& assetsDir, Diagnostics& diag)
    {
        GameData data;
        data.assetsDir = assetsDir;
        if (!data.assetsDir.empty() && data.assetsDir.back() != '/' && data.assetsDir.back() != '\\')
            data.assetsDir += '/';

        const std::string gameFile = "data/game.json";
        const auto root = detail::loadJsonFile(data.assetsDir + gameFile, gameFile, diag);
        if (!root) return data;
        parseGame(*root, gameFile, data, diag);

        if (!root->contains("files") || !(*root)["files"].is_object())
        {
            diag.error(gameFile + ": files: expected { \"items\": ..., \"barks\": ..., \"dialogue\": [...], \"rooms\": [...] }");
            return data;
        }
        const Json& files = (*root)["files"];
        const std::string where = child(gameFile, "files");
        detail::checkKeys(files, {"items", "barks", "dialogue", "rooms"}, where, diag);

        const auto load = [&](const std::string& file, auto&& parse) {
            if (const auto json = detail::loadJsonFile(data.assetsDir + file, file, diag)) parse(*json, file);
        };

        if (const auto file = detail::readString(files, "items", where, diag); !file.empty())
            load(file, [&](const Json& json, const std::string& f) { parseItems(json, f, data, diag); });
        if (const auto file = detail::readString(files, "barks", where, diag); !file.empty())
            load(file, [&](const Json& json, const std::string& f) { parseBarks(json, f, data, diag); });
        for (const auto& file : detail::readStrings(files, "dialogue", where, diag))
            load(file, [&](const Json& json, const std::string& f) { parseDialogue(json, f, data.conversations, diag); });
        for (const auto& file : detail::readStrings(files, "rooms", where, diag))
        {
            load(file, [&](const Json& json, const std::string& f) {
                if (!requireObject(json, f, diag)) return;
                for (const auto& [id, obj] : json.items())
                {
                    if (!id.empty() && id[0] == '_') continue;
                    if (data.rooms.contains(id))
                    {
                        diag.error(f + ": " + id + ": room \"" + id + "\" is already defined in " + data.rooms.at(id).file);
                        continue;
                    }
                    data.rooms[id] = parseRoom(id, obj, f, diag);
                }
            });
        }
        return data;
    }

    void checkGameData(const GameData& data, const std::set<std::string, std::less<>>& commands, Diagnostics& diag)
    {
        Checker(data, commands, diag).run();
    }
} // namespace hollow
