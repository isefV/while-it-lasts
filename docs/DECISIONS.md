# The Last Light: decisions

Step 0 of the project plan, answered on 13–14 October, and what the
prototype does with each answer. Change this file when a decision changes.

| # | Question | Decision | In the prototype |
|---|---|---|---|
| 1 | Language | English | All text is English; raylib's pixel font until `assets/fonts/` has one |
| 2 | Scope | All five stages. Stages 02 and 03 may grow after the prototype | Stage 02: checkpoint stealth room, barricade, sheltered wall. Stage 03: five mountain screens with dragging |
| 3 | Bars | None for now; maybe later | `game.json` → `bars` is empty; adding one is one line there |
| 4 | Movement and input | Y walks with the keyboard; use things with E or the mouse; no walking during dialogue | A/D or arrows walk; E uses the nearest thing; a click works only when standing next to it |
| 5 | Names of X and Y | Later | Written once in `game.json` → `characters`; `{x}` / `{y}` in every line |
| 6 | A dialogue entry | Simply text; thoughts in another colour | Text, plus an optional `if` and `do` where the story needs them (docs/DATA-FORMAT.md) |
| 7 | Text speed, skipping | Letter by letter; a press goes on | First press shows the whole line, second goes to the next |
| 8 | Palette | Dark and cold | `game.json` → `ui`; one warm accent (`#e0873a`) for fire, ember and sunset only |
| 9 | JSON mistakes | The data is written from the script | `hollow_app --check` (also in CTest) names the file and line or place of every mistake |
| 10 | Inventory | Opens on a key | Tab or I. Items are used through options at the target ("Use the key.") |
| 11 | Encounters | Some choices are timed | Factory creature (7 s, 4 s if it heard the cabinet), checkpoint chase (3 s), mountain creature (6 s) |
| 12 | Doing badly | The story changes a little | See "Consequences" below |
| 13 | Endings | Based on the player's choices | Stage 04 branches on cigarettes saved, cigarettes shared, notes read |
| 14 | Visible results | Yes, but not obviously | Every consequence below shows up later in a line or a scene |
| 15 | Rules | JSON first; rectangles until art; systems in the library, game in apps/app | Done |

## Consequences

| Choice or failure | Later |
|---|---|
| Cigarettes taken / left (pack, cabinet, backpack, wall) | Shared at the factory and the wall; a saved one at the summit, or X's own from his blanket |
| Shared two or more | "Thanks for all the cigarettes" at the summit |
| Both factory notes read | The extra line about the note at the summit (`notes_read >= 2`) |
| Too slow to leave the campsite | Branches snap, then a silhouette: `x_condition -1` |
| Antibiotics (bottom shelf, then the box under the sink) | `x_condition +1` and X's lines at the factory entrance |
| Bat on the cabinet | The creature starts alert and faster; sneaking past it fails |
| Seen on the factory floor | The creature guards the backpack: its bandages and cigarette are lost |
| Fought with the bat | Y is hurt; X notices ("You're bleeding.") |
| Spotted at the checkpoint | 2nd time `x_condition -1`; 3rd time a chase, and a stumble if too slow (`-1`) |
| `x_condition` | X's speed when following, X's idle lines, his lines on the first landing |

## Lines that are not in the script

The script has no lines for some moments the game needs (a second look at an
object, hints, the mountain creature, variants without bandages). Each such
line has `"_added"` next to it in the dialogue files, so the writer can find
and rewrite them.

## Open

- Names of X and Y.
- Bars: which, if any.
- Font, sounds and music (the game plays them as soon as files with the
  right names are in `assets/sounds/`: fire, crickets, wind, low_call,
  branches, dirt, metal, sheet, static, sting, click, crack, heavy_steps,
  machines, clang, thud, shot, roar, groan, alert, grind, lighter, leaves,
  insects, stream, footsteps, clatter, scrape, breath, inhale, ember).
- `cig_count` goes up to 4, not 3 as in the script's table: four cigarettes
  can be found, and a saved one should never disappear.
