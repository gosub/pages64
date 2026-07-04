# Swing — design

Groove for the whole chain, decided in the July 2026 brainstorm. The starting
question: the usual patch clocks Base64 at ×4 (16ths) and each page module
subdivides with its clock divider — where can swing live without breaking
that?

## Why not a swung external clock

Swing at the 16th level moves only the odd-numbered ticks. Feed a pre-swung
×4 clock into today's chain and every divider's output depends on its phase:
÷2 catching ticks 0,2,4 comes out straight (correct, MPC-style — 8ths don't
swing), ÷2 catching 1,3,5 comes out straight but uniformly late, ÷3
alternates and limps. Phase is only deterministic after RESET, and
tempo-measuring modules (Mlr64 varispeed) see an oscillating period and
wobble. Upstream swing is uncontrollable from inside the system.

## Decision: swing lives in Base64, applied to the broadcast

Base64 keeps taking a **straight** clock. It measures the incoming tick
period (median-of-3 + smoothing, the proven Mlr64 recipe, now moved into
Base64 and broadcast as `LeftMessage::clockPeriod`), counts tick parity from
RESET, and **delays the odd ticks/groups of the broadcast `clockTick`** by
`(swing − 50%) × 2 × unit × period`, capped inside the pair.

- **Swing amount** (menu): Off (50%), 54, 58, 62, 66 (triplet), 70, 75%.
- **Swing unit** (menu): every 2nd tick (= 16th swing at a ×4 clock) or
  every 2nd pair (= 8th swing at a ×4 clock). Base64 cannot know what a tick
  means musically, so the user says.

The per-module clock dividers need **zero changes**: after RESET they count
from tick 0, so even divisions consume only strong (undelayed) ticks and
stay straight while tick-level modules swing — the drum-machine-correct
outcome, emerging from the existing divider convention.

## Protocol addition

`LeftMessage` gains `float clockPeriod` — seconds per *straight* tick,
0 until measured. Forwarding is already a whole-struct copy, so it reaches
the whole chain. Mlr64 drops its private tempo measurement and reads the
broadcast (its median/smoothing logic moves to Base64 verbatim); this is
what keeps varispeed stable under swing, since Base64 measures the raw
input, not the swung broadcast. Bounce64 counts ticks rather than measuring
seconds, so it inherits the lilt naturally and needs nothing.

## Edge cases

- **No period measured yet** (`clockPeriod == 0`): swing disabled, ticks
  pass straight through.
- **RESET**: re-zeros parity and drops a pending delayed tick.
- **Tempo jump while a delayed tick is pending**: the pending tick fires
  immediately, the new one is scheduled.
- Raw `clockVoltage` stays raw — anything reading voltage instead of
  `clockTick` bypasses swing by design.
- Patched in mid-performance without RESET, a module may sit on odd parity
  (uniformly late, still even). Documented: RESET lines up the groove.

## Later (not now)

The mechanism is "delay tick *i* by table[i mod n] × period". Pairs are the
v1 table; MPC-style multi-step shuffle templates or seeded humanize-jitter
(the plugin's seeded-randomness contract fits) can reuse it without protocol
changes.

## Version

Ships as a patch bump (enhancement to Base64 + Mlr64 migration).
