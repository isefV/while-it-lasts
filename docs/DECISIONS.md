# The Last Light: decisions

Step 0 of the project plan, answered on 13–14 October, and what the
prototype does with each answer. Change this file when a decision changes.

| # | Question | Decision | In the prototype |
|---|---|---|---|
| 1 | Language | English | All text is English; raylib's pixel font until `assets/fonts/` has one |
| 2 | Scope | All five stages. Stages 02 and 03 may grow after the prototype | Stage 02: checkpoint stealth room, barricade, sheltered wall. Stage 03: five mountain screens with dragging |
| 3 | Bars | Changed 15 October: two, Y's health and X's condition | `game.json` → `bars`: `y_health` (0 to 5) and `x_condition` (0 to 3), top left; a bar flashes when it changes |
| 4 | Movement and input | Keyboard only (changed 15 October: no mouse); use a thing by standing next to it; no walking during dialogue | A/D or arrows walk; the nearest thing in reach shows an "E" prompt and E uses it; menus, options and the inventory use the arrow keys, E and Enter; the cursor is hidden. Added 15 October: G takes hold of X and lets go; F, R, Q swing, shoot and dodge in a fight |
| 5 | Names of X and Y | Later | Written once in `game.json` → `characters`; `{x}` / `{y}` in every line |
| 6 | A dialogue entry | Simply text; thoughts in another colour | Text, plus an optional `if` and `do` where the story needs them (docs/DATA-FORMAT.md) |
| 7 | Text speed, skipping | Letter by letter; a press goes on | First press shows the whole line, second goes to the next |
| 8 | Palette | Dark and cold | `game.json` → `ui`; one warm accent (`#e0873a`) for fire, ember and sunrise only |
| 9 | JSON mistakes | The data is written from the script | `hollow_app --check` (also in CTest) names the file and line or place of every mistake |
| 10 | Inventory | Opens on a key | Tab or I. Items are used through options at the target ("Use the key.") |
| 11 | Encounters | Some choices are timed; changed 15 October: fights are real time | Factory creature (7 s, 4 s if it heard the cabinet), checkpoint chase (3 s), mountain creature (6 s). "Fight it", or being too slow, starts a real-time fight (`apps/app/fight.cpp`); waves of crawlers at the barricade and on the broken stairs |
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
| Hit in a fight | `y_health` goes down, Y walks slower at 2 and below; X notices ("You're bleeding."). Bandages, antiseptic and painkillers give it back |
| Lost a fight | Factory: the creature guards the backpack. Barricade: X drives the crawlers off, `x_condition -1`. Stairs: it leaves anyway |
| A crawler reaches X | `x_condition -1`, once a wave; not while he is dragged or behind cover |
| Too slow in a near-death scene | The Reaper touches X: `reaper_touched +1`, `x_condition` 0, other lines after |
| X left alone on the mountain | The Reaper walks to him; if it reaches him, `x_condition -1` (once a room) |
| Spotted at the checkpoint | 2nd time `x_condition -1`; 3rd time a chase, and a stumble if too slow (`-1`) |
| `x_condition` | X's speed when following, X's idle lines, his lines on the first landing, the X bar |
| Bottles kept | Thrown at the checkpoint, the creature goes off into the trees for 12 s: no sneaking needed |

## Playtest notes, 15 October

| # | Note | What changed |
|---|---|---|
| 1 | Campsite music too noisy; a quiet midnight jungle | No music at the campsite. A quiet night loop (`jungle_night`: far frogs and night birds, crickets, a pond) and the fire at 45 % |
| 2 | Y aware at the end, no lying; they had a plan | `summit.cigarette_scene` and `summit.reaper` rewritten ("_changed" marks each line). The plan is set up at the barricade and the Collapse ("Up there. Like we said.") |
| 3 | X dying, a look at the Reaper, shaky screen, Y revives him | The `revive` scene, twice: at the barricade (7 presses, 15 s) and, worse, at the Collapse (10 presses, 12 s) |
| 4 | Stealth more optional | At the checkpoint a thrown bottle or bolt sends the creature into the trees; bottles at the campsite, the factory floor and the checkpoint |
| 5 | More things to inspect | 26 new: the moon, the jacket, the map, the pot, the tracks, the vending machine, the booth, the mug, the locker, the poster, the claw marks, the lunchbox, the warning sign, the boots, the car, the posters, the graffiti, the fire ring, the two lanterns at the foot, the stream, the shrine, the footprints, the edge, the scratched stone, the sky |
| 6 | Items more useful | Inventory E: bandages, antiseptic, antibiotics and painkillers heal X or Y; bottles and the bolt distract. Found in the booth, the locker, the cabinet, the backpack |
| 7 | Combat more immersive | Real-time fights: swing, shoot, dodge, the "!" before a lunge, hits, knockback, shake, red at the edges |
| 8 | More of the Reaper; monsters hurt | It shows up in every stage, closer each time, and follows on the mountain; fights cost `y_health` and `x_condition` |
| 9 | Sunrise, not sunset | They leave at midnight; the climb and the summit get lighter, and the sun comes up in the final image ("sunrise" script) |
| 10 | Monster waves | Two waves at the barricade (the slab's noise), one before the creature on the broken stairs |
| 11 | Grab and release X | G, whenever X cannot walk (`x_down`): after the barricade and from the Collapse on |

## Lines that are not in the script

The script has no lines for some moments the game needs (a second look at an
object, hints, the mountain creature, variants without bandages). Each such
line has `"_added"` next to it in the dialogue files, so the writer can find
and rewrite them.

## Open

- Names of X and Y. The bar labels in `game.json` → `bars` are the names'
  first letters; change them with the names.
- Font.
- Sounds and music are in (15 October): every sound the data names is in
  `assets/sounds/`, with a music track per stage, on the title screen and on
  the end card; the final ascent and the summit have no music (the script's
  "near silence"). Changed 15 October: the campsite has no music, only the
  night (`jungle_night`). Sources and licences: `assets/sounds/CREDITS.md`. The music (Kevin MacLeod) and a few
  recordings are CC BY, so the game's credits must name them.
- `cig_count` goes up to 4, not 3 as in the script's table: four cigarettes
  can be found, and a saved one should never disappear.
