# VirtualPad — Macro System

Macros can be defined in two ways:

**Option A — inline in `data/controllers.json`** (the macro lives alongside the button):
```json
"15": { "type": "macro", "name": "MacroName", "execution": "A, B, X" }
```

**Option B — in the library `data/macros.json`** (reusable from multiple buttons/controllers):
```json
[
  { "name": "MacroName", "execution": "A, B, X" }
]
```
```json
"15": { "type": "macro", "name": "MacroName" }
```

If the button has an inline `"execution"`, it is used directly. Otherwise, the engine looks up the macro by `"name"` in `macros.json`.

[Leer en español](MACROS.es.md)

---

## Tokens — Buttons

| Token | Button                    |
|-------|---------------------------|
| `A`   | A                         |
| `B`   | B                         |
| `X`   | X                         |
| `Y`   | Y                         |
| `L1`  | LB (left bumper)          |
| `R1`  | RB (right bumper)         |
| `L2`  | LT (left trigger)         |
| `R2`  | RT (right trigger)        |
| `L3`  | LS (left stick click)     |
| `R3`  | RS (right stick click)    |
| `CU`  | D-pad up                  |
| `CD`  | D-pad down                |
| `CL`  | D-pad left                |
| `CR`  | D-pad right               |
| `CUR` | Diagonal up-right         |
| `CUL` | Diagonal up-left          |
| `CDR` | Diagonal down-right       |
| `CDL` | Diagonal down-left        |
| `ST`  | Start                     |
| `SE`  | Select / Back             |
| `HO`  | Home (ignored — not injectable on virtual Xbox 360) |

---

## Tokens — Analog

Format: `LAX`, `LAY`, `RAX`, `RAY` followed by a float value between `-1.0` and `1.0`.

| Token   | Axis                         |
|---------|------------------------------|
| `LAX`   | Left stick, X axis           |
| `LAY`   | Left stick, Y axis           |
| `RAX`   | Right stick, X axis          |
| `RAY`   | Right stick, Y axis          |

**Value convention:**
- X: `-1.0` = full left · `0` = center · `+1.0` = full right
- Y: `-1.0` = full down · `0` = center · `+1.0` = full up

**Diagonals (unit circle):** use `±0.71` (= cos/sin of 45°), not `±0.5`.
`0.5+0.5` gives the right angle but the stick only reaches 70% of travel.

---

## Operators

| Syntax         | Meaning                                                     |
|----------------|-------------------------------------------------------------|
| `A, B, C`      | Sequence: press A, then B, then C (200ms between each)      |
| `A + Y`        | Combo: A and Y at the same time                             |
| `B=1000`       | Hold: keep B pressed for 1000ms                             |
| `500`          | Wait: pause of 500ms (bare number in the sequence)          |
| `A*5000`       | Repeat A for 5000ms (default interval: 200ms)               |
| `A*1000/10`    | Repeat A 10 times in 1000ms                                 |
| `A*UP`         | Repeat A while the physical button is held                  |
| `A*DO`         | Toggle: starts on press, stops on next press                |
| `(A, B)*5000`  | Repeat the sequence A,B in a loop for 5000ms                |
| `(A, B)*UP`    | Repeat the sequence while the button is held                |
| `(A, B)*DO`    | Toggle the sequence                                         |
| `(A, B)*1000/5`| Repeat the sequence 5 times in 1000ms                       |

**Combinable:** `A, 200, (B + X)*3000` — press A, wait 200ms, then loop B+X for 3s.

---

## Default timings

| Constant           | Value  | Description                                        |
|--------------------|--------|----------------------------------------------------|
| `DEFAULT_STEP_MS`  | 200ms  | Slot for each item in a sequence (provisional)     |
| `DEFAULT_PRESS_MS` | 80ms   | Press duration within the slot                     |

> The 200ms value is provisional. Measure the user's actual average press time
> and adjust `DEFAULT_STEP_MS` in `MacroParser.h`.

To use a duration other than the default: `A=150` (hold+slot = 150ms).

### D-pad steps suppress physical input

When a macro step includes any d-pad token (`CU`, `CD`, `CL`, `CR`, `CDR`, …), the macro **takes full ownership of the d-pad** for that step. Physical d-pad input from the player is suppressed.

This is intentional: a directional macro needs to control movement precisely. If the player is pressing right and the macro sends down, the game must receive pure down — not the diagonal that a simple OR would produce.

Macros with **no d-pad tokens** are unaffected — physical d-pad passes through normally.

### Minimum step duration for games

Most games run at 60 fps and read input once per frame (~16ms). **If a step is shorter than ~16ms, the game may not register it.**

- Safe minimum per direction step: **~40ms** (≈ 2–3 frames; accounts for polling jitter)
- Safe minimum for the button press: **~80ms** (≥ 5 frames)

For fighting game motions, use `=` to reduce both hold and slot:

```
CD=80, CDL=50, CL + Y=100
```

> Without `=`, each step defaults to 200ms — correct for menus and cutscenes,
> but too slow for special moves. `A=80` sets both the hold duration and the
> slot (time before the next step starts) to 80ms.

---

## Tested examples

### Auron Overdrive (FFX)
Button sequence for the overdrive. Adjust buttons and timings to match the game's actual sequence.
```json
          "name": "BanishingBlade",
          "execution": "CU, L1, CD, R1, CR, CL, Y"
```

### Lulu Overdrive (FFX)
Rotates the right analog stick through 8 points on the unit circle, ~4 rotations/second.
Each position is held for 30ms. Runs for 10 seconds and stops automatically.
```json
		  "name": "LuluOverdrive",
          "execution": "A, 1500,(RAX0+RAY1=17, RAX0.383+RAY0.924=17, RAX0.71+RAY0.71=17, RAX0.924+RAY0.383=17, RAX1+RAY0=17, RAX0.924+RAY-0.383=17, RAX0.71+RAY-0.71=17, RAX0.383+RAY-0.924=17, RAX0+RAY-1=17, RAX-0.383+RAY-0.924=17, RAX-0.71+RAY-0.71=17, RAX-0.924+RAY-0.383=17, RAX-1+RAY0=17, RAX-0.924+RAY0.383=17, RAX-0.71+RAY0.71=17, RAX-0.383+RAY0.924=17)*7000",
```

> Result: 7 rotations achieved (manual maximum ~6). ✓

### Soul Calibur grab
Two buttons that are difficult to press simultaneously on a PlayStation controller.
```json
"execution": "A + Y"
```

### Hadouken (Street Fighter / fighting games)
Facing right. Quarter circle forward + punch/kick.
D-pad motion: down → down-right → right + button.

Each direction must be held long enough for the game to register it (≥ 2 frames at 60 fps ≈ 30ms). Use `=` to reduce both hold and slot per step.

```
CD=80, CDR=50, CR + X=100
```

Facing left (mirror):
```
CD=80, CDL=50, CL + X=100
```

`CDR` / `CDL` are single diagonal tokens (down-right / down-left) — they press both directions simultaneously and are not the same as `CD + CR` (which would be a combo, not a sequence).

The default 200ms per step works for Konami codes and menus but is too slow for fighting game specials.

### Konami Code
```json
"execution": "(CU, CU, CD, CD, CL, CR, CL, CR, B, A, ST)*1"
```
> With `*1` it executes once (1ms total time, the sequence completes in full).
> Alternative without repeat: `"CU, CU, CD, CD, CL, CR, CL, CR, B, A, ST"`

### Button mash (Chun-Li style)
Press A repeatedly while the physical button is held.
```json
"execution": "A*UP"
```

### Mash with exact speed
10 presses of A in 1 second.
```json
"execution": "A*1000/10"
```
