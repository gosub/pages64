# Inertia64 — design (pass 1)

Eight rising masses with inertia, played with accelerator and brake pedals.
Version target: 2.12.0. Status: **drafted 2026-06-12**; concept **revised
2026-06-13** (rising-and-wrapping mass instead of a spinning disc; gentler
pedals — see §1 and the revision note).

## Scope summary

Each column is a mass that accelerates *upward*; its position wraps modulo
the grid, so when the cursor leaves the top it reappears at the bottom — a
rising sawtooth, not a sine. The top four pads of the column are throttle
pedals, the bottom four are brake pedals, each row a different intensity;
both act only while held. Per column the module outputs the position (a
0–10 V rising ramp whose frequency is the mass's speed) and the speed. Two
8-channel poly outputs (POS, VEL). No clock — the physics is continuous.
Title **INRT64**.

## 1. Physics

- Per column: position `y` (fraction of a full grid traversal, wraps 0–1)
  and speed `v` (traversals per second). Integrated per sample:
  `y += v · dt`, then `y −= floor(y)` so it wraps — the mass rising off the
  top reappears at the bottom.
- **Zero passive friction.** A moving mass keeps moving forever (the Flin64
  spirit — set it going, play something else); brakes are the only way to
  slow down. Masses keep moving while another page is active.
- **v is clamped to [0, vmax]** — no reverse; braking stops at standstill.
  `vmax` from the *Max speed* menu {1, 2, 4, 8} traversals/s, default **2**
  (the POS sawtooth's frequency in Hz; slow drift at the bottom, brisk LFO
  at the top).

> **Revision (2026-06-13).** The first draft modeled a spinning disc and read
> a rim point as a sine; the playing feel wanted was simpler — a mass that
> just rises and wraps. The sine cursor and the ±5 V rim-point output are
> gone; POS is now the raw wrapping position (a ramp). The pedals also gave
> too much energy at every level, so the rates were cut: see §2.

## 2. Pedals (the grid)

- **Rows 1–4 = throttle, rows 5–8 = brake**, intensity grows toward the
  edges (top row = hardest throttle, bottom row = hardest brake; the middle
  rows are the gentle pedals).
- Each pedal is rated by the **time to reach full speed** from rest (and to
  shed it under braking); the acceleration is `vmax / time`, so the pedal
  feel is the same at any max-speed setting. The times, outermost row to
  innermost:

  | Rows | Pedal | Time to full |
  |---|---|---|
  | 1 / 8 | hardest | 3 s |
  | 2 / 7 |        | 6 s |
  | 3 / 6 |        | 12 s |
  | 4 / 5 | gentlest | 24 s |

  These are deliberately heavy — a massive flywheel. The earlier draft's
  rates were ~8× stronger and overwhelmed the grid; even a quick tap of the
  gentlest pedal now imparts only a tiny nudge.
- **Momentary only:** a pedal acts while held, on press/release. Several
  pedals held in one column: the strongest held throttle and the strongest
  held brake both apply (riding gas and brake is allowed and useful — net
  acceleration is their difference).
- Held pedal pads are lit in the pedal color.

## 3. Scene buttons — handbrake + home (two-stage)

Scene A–H, per column:

- **Tap a moving column → instant stop** (`v = 0`; the position stays where
  it froze). The panic/punctuation gesture — eight masses take a while to
  brake by pedal.
- **Tap a stopped column → send it home** (`y = 0`, the bottom). So a double
  tap stops then re-zeros; a single tap of an already-idle column lines it
  up at the start. RESET still homes all columns at once.

Scene LED lit while the column is moving (a speed overview at the grid's
edge), off when stopped — so the LED also tells you which tap you'll get.

## 4. Display

The column's cursor is the mass itself: one lit pad at `y`, rising up the
column and wrapping to the bottom. The faster the mass, the faster the pad
climbs — speed is readable at a glance. Cursor in the cursor color; held
pedals overlay in the pedal color.

## 5. Outputs, reset, menu

- **POS** (poly 8ch): `10 V · y` per column — a unipolar 0–10 V rising
  sawtooth (bottom = 0 V, top = 10 V), frequency = the mass's speed. A
  phasor you push and drag rather than set. **Declicked** by default: the
  output is slewed at 10000 V/s (the full 0–10 V swing in ~1 ms) so the
  sawtooth's wrap edge — and a RESET or home jump — round off instead of
  clicking. The rising ramp (≤ 80 V/s at max speed) is far below the slew
  ceiling, so it passes through untouched. Menu toggle, on by default.
- **VEL** (poly 8ch): `10 V · v / vmax`, unipolar.
- **RESET tick:** all masses stop and re-zero (`v = 0, y = 0`).
- No clock divider — nothing is clocked.
- Menu: **Max speed** {1, 2, 4, 8} traversals/s, **Declick POS output**,
  **cursor color**, **pedal color**.
- Serialized: `v[8]`, `y[8]`, max speed, declick, colors. Transient: held
  pedals, slewed POS output.

## 6. Panel

10 HP, page grammar, title **INRT64**. Two badged jacks: POS, VEL.

## 7. Implementation stages (each a commit, panel first as usual)

1. **Core:** panel, per-sample physics, pedal handling, POS/VEL outputs,
   cursor + pedal LEDs, serialization.
2. **Polish:** scene handbrakes + moving LEDs, max-speed menu, docs page,
   README entry, version bump 2.12.0.

## Resolved decisions (2026-06-12, revised 2026-06-13)

1. **Rising mass, position wraps modulo the grid** (revised) — a vertical
   sawtooth, not a spinning disc / sine.
2. **Zero passive friction** — momentum persists, brakes are the only way down.
3. **No reverse** — `v` clamps at 0.
4. **Pedal intensity grows toward the grid edges**, gentle pedals in the
   middle; rates set as time-to-full-speed and cut ~8× from the first draft
   (revised).
5. **Strongest-held-pedal wins per side**, gas and brake may overlap.
6. **POS is the wrapping position, unipolar 0–10 V** (revised); VEL unipolar
   0–10 V. **POS is declicked** by a 1 ms output slew, on by default
   (added 2026-06-13).
7. **Scene buttons are two-stage per column**: tap-to-stop when moving,
   tap-to-home when stopped; lit while moving (home added 2026-06-13).
