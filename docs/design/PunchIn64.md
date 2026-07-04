# Rhythm64 punch-in FX — design

PO-style punch-in effects for Rhythm64, decided in the July 2026 brainstorm.
Because Rhythm64 emits gates, the effects are **rhythm-domain** — time and
pattern manipulation, the sequence half of the Pocket Operator FX set. Audio
punch-ins would belong on the kit companions (not designed).

## Gesture

**Hold scene B** (momentary): the grid repaints as the effect selector.
**Rows are effects, columns are the amount** — one press picks both. The
effect is active while B *and* a pad are held; last pressed pad wins;
releasing either ends it. Nothing latches (punch-in is performance — the
button-7 gesture recorder is the future home of repeatable versions) and
nothing serializes.

Scene-button convention check: interactive play only — a momentary
performance effect is exactly that. A is latch, B is punch-in, C–H stay
free.

## Effect table

| Row | Effect | Columns |
|---|---|---|
| 1 | Loop | roll the **last** 1, 2, 3, 4, 6, 8, 12, 16 steps (n=1 = repeat the step you just heard) |
| 2 | Ratchet | every hit becomes ×2 … ×24 sub-hits across the step |
| 3 | Time | ÷4, ÷3, ÷2 · reverse · ×2, ×3, ×4, ×8 |
| 4 | Density | bipolar: thin (keep 15/33/55/75%) ← → fill (+1…+4 shifted copies) |
| 5 | Mask | kicks only → low half … top half → hats only |
| 6 | Shuffle | reorder time slices in a window of 2 → whole loop |
| 7 | Push/drag | hits early ← → late, up to ~half a step |
| 8 | *(spare)* | inert — reserved for what performing reveals is missing |

Decisions from the brainstorm:

- **Rotate was cut**: as a momentary effect it reads as a skip (jump on
  press, jump on release), which is an edit, not a groove. Push/drag —
  sub-step timing lean, audible and continuous — took its place.
- **Density is one bipolar row** (thin left of center, fill right), not two.
- **Shuffle keeps verticality**: one shared random step index per tick for
  all 64 pads, so co-occurring hits stay together — time slices reorder, the
  drum kit doesn't fall apart. Column bounds the jump window; small windows
  groove, full-loop is the emergency lever.
- Row 8 stays honestly empty rather than filled with a weak effect.

## Architecture

All effects are **readout transforms** over an untouched engine: the shared
step counter keeps running in global time underneath, patterns are never
mutated, and release drops you back exactly where the band is (drift-free,
the property that makes punch-ins safe to slam).

- Step-remap effects (loop, reverse, ÷n, shuffle) compute an effective step
  from an anchor captured at pad-press.
- Sub-step effects (ratchet, ×n, push/drag) schedule extra or delayed fires
  through a small sample-countdown queue, timed from the broadcast
  `clockPeriod` × the module's clock divider.
- Per-hit effects (density, mask) filter inside the fire routine, so queued
  fires inherit them.
- Drag (early) is causal: on each step it pre-fires the *next* step early
  and suppresses it at its real tick. Punch-out mid-pair can flam once —
  accepted artifact.

LEDs while B is held: selectable cells dim amber, the active cell bright
amber, spare row dark; scene B lights amber. Release repaints the rhythm
view (the page-select repaint machinery).

## Later (not now)

The same overlay could grow on Step64 / Euclid64 / Cafe64 as a shared
convention, and a `sharedKey`-style atomic could let one gesture also engage
an audio punch-in on the kit companions.

## Version

Ships as a patch bump (enhancement to Rhythm64).
