# The Last Light: data format

Everything the story needs is in `assets/data/`. Writing and art never wait for
code: a new line, choice, room object or item is a change to these files only.

```
assets/data/
├── game.json             names, story variables, colours, the list of files below
├── items.json            inventory items
├── barks.json            short lines over a character's head
├── dialogue/items.json   what using an item from the inventory does
├── dialogue/stage00.json … stage04.json   conversations
└── rooms/stage00.json … stage04.json      rooms and what is in them
```

**Checking your work.** Run `hollow_app --check` (or the `CMake: ctest` task).
It reads every file and prints each mistake with its file and place, for
example:

```
error: data/dialogue/stage02.json:7:28: syntax error while parsing object key - unexpected ','
error: data/dialogue/stage02.json: shelter.pack[8].choice[1].next: unknown conversation "shelter.sav"
error: data/dialogue/stage02.json: barricade.rod[0].do "rod_takn = true": unknown variable "rod_takn"
```

`shelter.pack[8].choice[1]` means: conversation `shelter.pack`, its 9th entry
(counting from 0), the 2nd option. Comments (`// ...`) are allowed anywhere,
and any key that starts with `_` is ignored (`"_added": "why this line is new"`).

**Names.** X and Y are placeholders. Their names are written once, in
`game.json` → `characters`. In any line, `{x}` and `{y}` become those names.

---

## 1. Dialogue

A dialogue file maps conversation ids to lists of entries. The game plays the
entries from the top.

```json
{
    "campsite.cigarettes": [
        { "y": "One left. Of course there's one left. Just one." },
        { "choice": [
            { "text": "Take it.", "do": ["cig_count += 1"], "next": "campsite.cig_taken" },
            { "text": "Leave it." }
        ] }
    ],
    "campsite.cig_taken": [
        { "thought": "Later. Maybe." }
    ]
}
```

### Entries

Each entry is exactly one of these:

| Entry | Example | What it does |
|---|---|---|
| a line | `{ "x": "Mm. Five more minutes." }` | The key is the speaker (a character id). Shown as `X: Mm. Five…` |
| a thought | `{ "thought": "Probably a good sign." }` | The player's inner voice: no name, its own colour |
| narration | `{ "narration": "{x} tries to sit up and winces." }` | What happens, in grey |
| a choice | `{ "choice": [ …options… ] }` | Up to 4 options on screen (see below) |
| a pause | `{ "pause": 1.5 }` | Nothing on screen for that many seconds |
| a branch | `{ "if": "has_antibiotics", "then": [ … ], "else": [ … ] }` | Plays one list or the other; `else` is optional |
| a jump | `{ "goto": "factory.leave" }` | Continues in another conversation |
| effects only | `{ "do": ["stage01_done = true"] }` | Changes things, shows nothing |
| the end | `{ "end": true }` | Stops the conversation here |

Any entry can also have:

- `"if": "<condition>"`: the entry is skipped while the condition is false.
- `"do": [ "<effect>", … ]`: applied when the entry is reached.

Text appears letter by letter. The first press (E, Space or Enter)
shows the whole line; the next goes on. A line too long for the box (about
4 lines of 50 letters) is split into pages by itself.

### Choices

```json
{ "choice": [
    { "text": "Use the key.",           "if": "item.key", "next": "office.cabinet_key" },
    { "text": "Force it with the bat.", "if": "item.bat", "next": "office.cabinet_bat" },
    { "text": "Leave it." }
] }
```

| Option key | Meaning |
|---|---|
| `text` | what the player reads |
| `if` | the option is shown only while true (items are used this way: an option that needs the item) |
| `do` | effects when picked |
| `next` | conversation to continue with; without it, the conversation goes on below the choice |
| `timeout` | `true`: taken when the time runs out |

A **timed choice** has `"timer": <seconds>` and one option with
`"timeout": true`. That option may have no `text`: it is then never shown and
only happens when the player is too slow.

```json
{ "choice": [
    { "text": "Run for the road!", "next": "checkpoint.escape" },
    { "timeout": true, "next": "checkpoint.stumble" }
], "timer": 3 }
```

---

## 2. Conditions and effects

**Conditions** (`if`, `exit_if`, `block`):

```
has_antibiotics                    true when not 0
!noisy_entry                       not      (also: not)
cig_count >= 1                     == != < <= > >=
x_condition <= 1 && item.bandages  and: &&  (also: and)
a || b                             or:  ||  (also: or)
(a || b) && !c                     parentheses
item.key                           1 while the key is in the inventory
seen.campsite.fire                 how many times that conversation has started
```

**Effects** (`do`), one per string:

```
cig_count += 1        x_condition -= 1        noisy_entry = true
give bandages         take key
```

Every variable must be declared in `game.json` → `variables` (the check
reports a misspelled one). A variable keeps inside its range:
`"x_condition": { "value": 3, "min": 0, "max": 3 }`, so `x_condition -= 5`
gives 0.

**Commands** for the game, also in `do`:

| Command | Does |
|---|---|
| `goto <room> [x]` | changes room (with a fade); put it last |
| `sound <id>` | plays `assets/sounds/<id>.ogg` (or .wav, .mp3, .qoa) if it exists |
| `ambient <id> <id> …` | the sound loops that play now (none: silence); `fire@0.4` plays a loop at 40 % |
| `music <id>` / `music stop` | music track |
| `pose <x\|y> <pose>` | idle, sit, lying, kneel |
| `companion follow\|stay [x]\|drag\|absent` | what X does |
| `place <x\|y> <x>` | puts a character at x |
| `step <x\|y> <dx>` | walks that far (negative: left) |
| `face <x\|y> left\|right` | turns a character |
| `shake <seconds> [strength]` | shakes the scene |
| `tint <#rrggbb> [seconds]` | colours the scene (night, sunrise) |
| `bark <pool>` | a short line over a head |
| `event <name> …` | something for the room's script (see the room files) |
| `fight` | starts the room's `fight` (see Rooms); put it last |
| `revive <conversation> [presses] [seconds]` | X stops breathing: the near-death scene (E on the beat, the Reaper walks in). `presses` good presses needed (8), `seconds` until the Reaper reaches him (14). Then the conversation plays; if the Reaper got there first, `reaper_touched += 1` and `x_condition = 0` |
| `reaper show <x> [height] [seconds]` | the Reaper stands at x; height 80 is near, 40 far away |
| `reaper hide [seconds]` / `reaper glimpse <x> [height]` | gone / there for a moment, with a sting |
| `reaper follow <distance>` / `reaper stop` | it walks behind them, that far back: it creeps up while they stand still, fades while they look back at it, walks to X when he is left lying alone, and costs `x_condition` once a room if it reaches him |
| `end` | fades out to the end card |

---

## 3. Rooms

```json
{
    "campsite": {
        "name": "The Campsite",
        "background": "backgrounds/campsite.png",
        "color": "#18202c",
        "tint": "#c4c8dc",
        "ambient": ["fire", "crickets", "wind"],
        "player_x": 118,
        "walk": [8, 312],
        "enter": [ { "dialogue": "campsite.start" } ],
        "companion": { "x": 268, "mode": "stay", "pose": "lying", "dialogue": "campsite.x" },
        "idle_barks": ["y_idle"],
        "script": "campsite",
        "objects": [ … ]
    }
}
```

| Room key | Meaning |
|---|---|
| `name` | shown in the box between conversations |
| `background` | 320 x 180 image; until it exists, `color` fills the scene |
| `tint` | multiplies the whole scene (night, dusk) |
| `ambient`, `music` | sound loops |
| `player_x` | where the player appears (an exit's `spawn_x` wins) |
| `walk` | `[from, to]`, how far the player can walk |
| `enter` | list of `{ "if", "dialogue", "do", "once" }`: on entering, every rule whose `if` is true applies its `do`; the first with a `dialogue` starts it. `once` (default true): only the first time |
| `companion` | X here: `x`, `mode` (stay, follow, drag), `pose`, `dialogue` / `again` when talked to, or `bark`; `release` / `grab`: conversations when the player lets go of him (G) or takes hold again |
| `creature` | the creature: `x`, `y`, `w`, `h`, `speed`, `color`, `sprite`, `patrol: [from, to]`, `phases`, `if` |
| `fight` | a real-time fight, started by the command `fight` (below) |
| `idle_barks` | bark pools for when the player stands still |
| `script` | code for this room (`campsite`, `creature`, `checkpoint`, `sunrise`, `summit`) |

### Fights

```json
"fight": {
    "brute": true, "hp": 5,
    "waves": [ { "crawlers": 2, "from": "left" }, { "crawlers": 3, "from": "both" } ],
    "win": "stairs.fight_won",
    "lose": "stairs.fight_lost"
}
```

The waves of small creatures ("crawlers") come one after the other from the
sides (`right`, `left`, `both`); then, with `"brute": true`, the room's
`creature` itself, with `hp` strength. The player fights in real time: **F**
swings the bat, **R** shoots (a round a shot, needs the pistol), **Q** dodges
back, A/D move. A creature shows a "!" just before it lunges. A lunge that
reaches Y costs `y_health` (1, the brute 2); crawlers coming from X's side go
for him, and one that reaches him costs `x_condition` (once a wave) unless he
is dragged or lies behind a `cover` object. Y cannot fight while dragging X:
G lets go of him first.

When every creature is beaten or gone, `win` plays. When `y_health` reaches
0, Y goes down, the creatures scatter and `lose` plays (Y is left with 1):
never game over. A brute still standing goes back to the room's script, so
`lose` can say what it does next (`event creature_guard 262`).

### Objects

Positions are scene pixels: the scene is 320 x 180, and characters stand on
the ground line at y = 150. An object without `"y"` stands on that line.
Every object is a coloured rectangle until its `sprite` exists; the highlight
when it can be used is `<sprite>_hover.png`, or a light outline.

| Object key | Meaning |
|---|---|
| `id` | unique in the room |
| `name` | shown over it when the player can use it |
| `x`, `y`, `w`, `h`, `color`, `sprite` | where and how it looks |
| `front` | drawn in front of the characters (hiding spots) |
| `hidden` | not drawn (zones) |
| `if` | it is there only while true (a cabinet closed / open is two objects) |
| `dialogue`, `again` | conversation when used; `again` for the second and later times |
| `once` | can be used one time |
| `interactive` | `false`: decoration. Default: true when it has a dialogue, exit or event |
| `exit`, `spawn_x` | room it leads to, and where the player appears there |
| `exit_if`, `locked` | the exit works only while true; otherwise `locked` plays |
| `trigger`, `repeat` | fires when the player walks into it (not on E); `repeat`: every time |
| `block`, `blocked` | the player cannot walk through while the condition is true; `blocked` plays when they try |
| `cover` | a hiding spot (checkpoint); in a fight, crawlers leave X alone behind one |
| `rest` | a rest spot while dragging X |
| `effect` | `fire`, `ember` or `glow`: light drawn by the game, fading when `if` turns false |
| `event` | sent to the room's script when used |

A player uses things with **E** while standing next to them: the nearest
usable thing within reach gets the "E" prompt and an outline, and only that
one can be used; when several are within reach, W/S (or ↑/↓) moves the
prompt between them. The game has no mouse input.

**X on the ground.** While `x_down` is true (set by the story: after a
near-death scene, and on the mountain), **G** next to him takes hold of him
(the companion mode `drag`) and G again lets go of him where he is. Exits
that need him (`"exit_if": "dragging"`) stay closed until he is held.

---

## 4. Items, barks, characters

`items.json`:

```json
"cigarette": { "name": "Cigarettes", "description": "Saved, not smoked. Not yet.",
               "icon": "items/cigarette.png", "color": "#d8d2c4", "count": "cig_count" }
```

`count` makes the item follow a variable (shown while above 0, with the
number) instead of `give` / `take`. `use` names a conversation that **E**
plays when the item is selected in the inventory (`dialogue/items.json`): a
bandage on X or on Y, the antibiotics, a bottle thrown at the checkpoint.
`{x}` and `{y}` work in names and descriptions.

`barks.json`: pools of short lines, never the same twice in a row. An entry is
one line or a short exchange:

```json
"hide": {
    "if": "x_condition >= 1",
    "lines": [
        [ { "x": "Don't breathe." }, { "y": "I'm not." } ],
        { "x": "I'm here. Just slow." }
    ]
}
```

`game.json` → `characters`: `name`, `color` (placeholder body),
`text_color` (name in the box), `w`, `h`, `speed`, `sprite` (sheet config,
see visual-asset-config.txt section 5).

`game.json` → `bars`: up to three bars top left, each
`{ "label", "var", "max", "color" }`. Two: Y's health (`y_health`, 0 to 5)
and X's condition (`x_condition`, 0 to 3). A bar flashes when it changes.

---

## 5. Testing a branch

```
hollow_app --room factory_office --set noisy_entry=true --set cig_count=2
hollow_app --walkthrough tests/walkthrough/full.txt
```

`--room` starts a new game in any room, `--set` presets variables. A
walkthrough plays the whole game by itself from a list of commands
(`use <object>`, `talk`, `choose <text>`, `item <id>`, `grab`, `fight`,
`revive`, `expect <condition>`, …; see
`apps/app/walkthrough.hpp`) and fails if the player gets stuck. Two are in
`tests/walkthrough/`: a careful route and a careless one; both run in CTest.
