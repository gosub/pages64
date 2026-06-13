# Inertia64 — design (pass 1)

Eight masses with inertia, played with up/down pedals. Version target:
2.12.0. Status: **drafted 2026-06-12**; **revised 2026-06-13** (rising-and-
wrapping mass instead of a spinning disc; gentler pedals); **bidirectional
lanes + Direction page added 2026-06-13** (see §1, §6 and the revision notes).

## Scope summary

Each column is a mass driven up or down; its position wraps modulo the grid,
so when the cursor leaves the top it reappears at the bottom (and vice versa)
— a sawtooth, not a sine. The top four pads push up, the bottom four push
down, each row a different intensity; both act only while held. A lane is
**monodirectional** (down pads brake to a stop) or **bidirectional** (down
pads drive it into reverse), and has a **friction** level that damps it toward
rest — both chosen per lane on config pages. Per column the module outputs the
position (a 0–10 V ramp whose frequency is the mass's speed) and the signed
speed. Two 8-channel poly outputs (POS, VEL). No clock
— the physics is continuous. Title **INRT64**.

## 1. Physics

- Per column: position `y` (fraction of a full grid traversal, wraps 0–1)
  and speed `v` (traversals per second, signed). Integrated per sample:
  `y += v · dt`, then `y −= floor(y)` so it wraps either direction.
- **Per-lane viscous friction.** Each lane has a friction level 0–8 (the
  Friction page). At level 0 there is no friction: a moving mass keeps moving
  forever (the Flin64 spirit), the default. Above 0 the speed decays as
  `v −= k·v·dt` with `k = FRICTION_MAX · level / 8` (`FRICTION_MAX = 2 /s`),
  so a held pedal settles at a **terminal speed = pedalAccel / k** (the
  pedals become speed setpoints — each intensity a distinct cruise speed) and
  an unpedaled mass coasts to a stop (snapped to exactly 0 below a tiny
  threshold). Masses keep moving while another page is active.
- **v is clamped to the lane range:** `[0, vmax]` for a monodirectional lane
  (a down pad brakes and stops at 0), `[−vmax, vmax]` for a bidirectional one
  (a down pad keeps pushing past 0 into reverse). `vmax` from the *Max speed*
  menu {1, 2, 4, 8} traversals/s, default **2** (the POS sawtooth's frequency
  in Hz; slow drift at the bottom, brisk LFO at the top).

> **Revision (2026-06-13).** The first draft modeled a spinning disc and read
> a rim point as a sine; the playing feel wanted was simpler — a mass that
> just rises and wraps. The sine cursor and the ±5 V rim-point output are
> gone; POS is now the raw wrapping position (a ramp). The pedals also gave
> too much energy at every level, so the rates were cut: see §2.
>
> **Bidirectional (2026-06-13).** Reverses the original "no reverse" decision.
> The down pads now keep pushing past 0 on bidirectional lanes, so they read
> as "accelerate down" rather than "brake"; only the lower velocity clamp
> differs between the two modes. VEL therefore becomes signed — a
> monodirectional lane still spans 0–10 V, a bidirectional one ±10 V.

## 2. Pedals (the grid)

- **Rows 1–4 push up, rows 5–8 push down**, intensity grows toward the
  edges (top row = hardest up, bottom row = hardest down; the middle rows are
  the gentle pedals). On a monodirectional lane the down pads can only brake
  to 0; on a bidirectional lane they continue into reverse.
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
  pedals held in one column: the strongest held up pad and the strongest held
  down pad both apply (pushing both ways at once is allowed — net acceleration
  is their difference).
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
- **VEL** (poly 8ch): `10 V · v / vmax`, signed — a monodirectional lane
  reads 0–10 V, a bidirectional one ±10 V. An **Absolute VEL** menu option
  emits `|v|` instead, so a bidirectional lane reads 0–10 V in both
  directions (speed without sign); default off.
- **RESET tick:** all masses stop and re-zero (`v = 0, y = 0`).
- No clock divider — nothing is clocked.
- Menu: **Max speed** {1, 2, 4, 8} traversals/s, **Declick POS output**,
  **Absolute VEL**, **cursor color**, **pedal color**, **active/inactive page
  color**.
- Serialized: `v[8]`, `y[8]`, `bidir[8]`, `friction[8]`, sub-page, max speed,
  declick, colors. Transient: held pedals, slewed POS output.

## 6. Sub-pages (top buttons)

Top round buttons select a page (the Cafe64 / Mlr64 idiom; buttons 1–5 are
sanctioned for config). Active page bright, others dim.

- **1 = Play** (default): the pedals.
- **2 = Direction:** tap anywhere in a column to toggle it mono ↔
  bidirectional. Display per column: the **top cell lit = goes up only**
  (monodirectional); **top + bottom cells lit = goes both ways**
  (bidirectional). Switching a moving bidirectional lane back to mono lets the
  next physics frame clamp any reverse velocity up to 0.
- **3 = Friction:** a bar per column, height = the friction level. Tap a row
  to set the level to that height (bottom = 1 … top = 8); tap the current top
  of the bar again to clear it to 0 (the Euclid64 idiom). Empty column = no
  friction.

Switching pages releases all held pedals (so none stick while you leave the
play surface); the physics keeps running on every page. Scene buttons act on
the play page only.

## 7. Panel

10 HP, page grammar, title **INRT64**. Two badged jacks: POS, VEL.

## Resolved decisions (2026-06-12, revised 2026-06-13)

1. **Rising/falling mass, position wraps modulo the grid** (revised) — a
   sawtooth, not a spinning disc / sine.
2. **Per-lane viscous friction** (0–8, default 0 = none): viscous, not
   constant, so held pedals become speed setpoints (terminal speed =
   accel/k); coasting masses snap to rest below a tiny threshold (added
   2026-06-13).
3. ~~No reverse.~~ **Per-lane mono/bidirectional** (revised): mono clamps `v`
   at 0, bidirectional allows reverse; set on the Direction page.
4. **Pedal intensity grows toward the grid edges**, gentle pedals in the
   middle; rates set as time-to-full-speed and cut ~8× from the first draft
   (revised).
5. **Strongest-held-pedal wins per side**, the two sides may overlap.
6. **POS is the wrapping position, unipolar 0–10 V** (revised); **VEL is
   signed** (mono 0–10 V, bidirectional ±10 V). **POS is declicked** by a
   1 ms output slew, on by default (added 2026-06-13).
7. **Scene buttons are two-stage per column**: tap-to-stop when moving,
   tap-to-home when stopped; lit while moving (home added 2026-06-13).
