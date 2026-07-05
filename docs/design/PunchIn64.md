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
| 2 | Ratchet | left half: ×N accent on every Nth step (2·3·4·6) · right half: gentle ×2·3·4·5 on every step |
| 3 | Time | ÷3, ÷2, ½-retrograde · reverse (ping-pong) · ×2, ×3, ×4, ×6 |
| 4 | Density | bipolar: thin (keep 15/33/55/75%) ← → fill (+1…+4 shifted copies) |
| 5 | Mask | kicks only → low half … top half → hats only |
| 6 | Shuffle | reorder time slices in a window of 2 → whole loop |
| 7 | Swing | drag (offbeats ahead) ← → push (offbeats laid back), up to ~0.4 step |
| 8 | Musical ratchet | sparse, mixed low multipliers (2–6, 8 rare) on a growing subset of steps |

Decisions from the brainstorm:

- **Rotate was cut**: as a momentary effect it reads as a skip (jump on
  press, jump on release), which is an edit, not a groove. A timing-lean row
  took its place.
- **The timing row is swing, not a uniform lean.** A first pass shifted every
  step by the same amount, which reads as a one-time lurch on entry/exit, not
  a groove (a constant offset is just latency). Groove is *relative*, so the
  row now delays only the alternating steps: push lays the offbeats back
  (classic shuffle), drag delays the downbeats so the offbeats feel pushed
  ahead. Delay-only, so it needs no fragile pre-fire.
- **Density is one bipolar row** (thin left of center, fill right), not two.
- **Shuffle keeps verticality**: one shared random step index per tick for
  all 64 pads, so co-occurring hits stay together — time slices reorder, the
  drum kit doesn't fall apart. Column bounds the jump window; small windows
  groove, full-loop is the emergency lever.
- **Two ratchet rows, both musical.** Ratcheting *every* step at high counts
  is a glitch texture that belongs in an audio effect, not the rhythm bus, so
  the rhythm-domain ratchets stay low and sparse. Row 2 splits in half: the
  right half puts a gentle ×2–×5 on every step, the left half puts a ×N accent
  on only every Nth step (on-grid, so it lands as a flam accent). Row 8 (once
  the honestly-empty spare) is the *musical ratchet*: a growing subset of steps
  ratchet with mixed low multipliers, deterministic per grid position so the
  same steps ornament each pass. The high-glitch end is deferred to the audio
  punch-in (Punch64) on the roadmap.

## Architecture

All effects are **readout transforms** over an untouched engine: the shared
step counter keeps running in global time underneath, patterns are never
mutated, and release drops you back exactly where the band is (drift-free,
the property that makes punch-ins safe to slam).

- Step-remap effects (loop, reverse, ÷n, shuffle) compute an effective step
  from an anchor captured at pad-press.
- Sub-step effects (ratchet, ×n, swing) schedule extra or delayed fires
  through a small sample-countdown queue, timed from the broadcast
  `clockPeriod` × the module's clock divider. Swing delays only the
  alternating steps, so it stays entirely inside this delay queue (no
  pre-fire, no tick suppression, no flam-on-release).
- Per-hit effects (density, mask) filter inside the fire routine, so queued
  fires inherit them.
- Reverse is a **ping-pong** window: it bounces back and forth across the
  last few steps rather than scanning the whole loop backward once, so it
  reads as a hooky retrograde figure instead of an arbitrary rewind.

## Tuning (2.21.2)

A listening pass softened the extremes that overpowered the groove: ratchet
tops out at ×16 (was ×24) with a gentler low-end ramp; the time row caps at
÷3 and ×6 (was ÷4 and ×8); reverse became the ping-pong above; and push/drag
became relative swing.

## Tuning (2.21.3)

The ratchet row was reworked away from a single every-step ×2…×16 sweep,
whose top end was a glitch texture more than a groove: row 2 is now the
split accent/gentle ratchet, and the freed spare row 8 became the musical
ratchet. The third division column, previously a duplicate ÷2, became
retrograde half-time (÷2 walking backward from the anchor).

LEDs while B is held: selectable cells dim amber, the active cell bright
amber; scene B lights amber. (All eight rows are now live, so none is dark.)
Release repaints the rhythm view (the page-select repaint machinery).

## Later (not now)

The same overlay could grow on Step64 / Euclid64 / Cafe64 as a shared
convention, and a `sharedKey`-style atomic could let one gesture also engage
an audio punch-in on the kit companions.

## Version

Ships as a patch bump (enhancement to Rhythm64).
