# Meadow64 — design (pass 1)

A meadowphysics-style cascading-counter sequencer: eight countdown counters
whose firings reset and bump each other into evolving cross-rhythms. Version
target: 2.14.0. Status: **drafted 2026-06-14**, expanding the ROADMAP
milestone (the cross-rules become a meadowphysics glyph page — see §3).

## Scope summary

Eight grid rows = eight countdown counters. Each counts down on the (divided)
clock; when it reaches the bottom it fires its trigger and reloads. On firing
a counter can also act on other counters — reset, speed up or slow them — and
those knock-ons can cascade within a tick, which is the whole point: simple
counters chained into complex, self-evolving patterns. A page module (orange),
title **MDW64**. 8 mono trigger outs + a poly out. Scene buttons mute.

## 1. Counters (the play page)

- Each **row r** is a counter with a **length** `L[r]` (1–8 ticks) and a
  current **count** `pos[r]` (0 … L−1). On each divided clock tick: if
  `pos == 0` the counter **fires** and reloads (`pos = L−1`); otherwise
  `pos--`. So a length-`L` row fires every `L` ticks — length is the period.
- **Set the length:** tap a pad in the row; the column sets `L` (col 1 = 1 …
  col 8 = 8) and reloads the counter there. The counter is a ball that walks
  leftward from its home column and fires off the left edge.
- **Display:** the moving count is a bright **cursor** at column `pos`; the
  **home** column (`L−1`, the reload point) shows as a dim marker so you can
  read the length even mid-travel; the row flashes on fire.

## 2. Scene buttons — mutes

Scene A–H mute rows 1–8 (the Euclid64/Bounce64 idiom). A muted counter keeps
counting and **keeps applying its cross-rules** — muting only gates the
trigger *output*, so the network's evolution is unchanged and the row
re-enters in phase. The cursor turns the mute color; the scene LED lights for
a muted row.

## 3. Cross-rules (the heart) — the rules page

Top button 2 opens the rules page, a meadowphysics-style glyph selector that
edits one **(source → destination)** pair at a time:

- **Scene buttons A–H select the source row** (whose firing triggers the
  rule); the chosen scene LED lights.
- **The leftmost grid column selects the destination row** (8 cells = rows
  1–8). The selected destination is drawn as a **dim horizontal line** across
  the grid — in the **background**, beneath the glyph; in the left column,
  destinations that already carry a rule from this source glow dim and the
  selected one bright.
- **The rest of the grid (columns 2–8) selects the rule:** press any cell and
  its **row** chooses the rule (the 8 grid rows = the 8 rules below). The
  current rule for the selected pair is drawn bright as a **6×6 glyph** in the
  centre of the grid, on top of the dim destination line.

The eight rules, applied to the destination counter when the source fires
(the ball runs leftward, **min** = the left/fire edge, **max** = the right/home
edge):

| Row | Rule | Glyph | Effect on the target |
|---|---|---|---|
| 1 | none | ╲ diagonal slash | nothing (default) |
| 2 | increment | ＋ plus | count +1 (fires one tick later) |
| 3 | decrement | － minus | count −1 (sooner; from min it fires now → cascade) |
| 4 | go to max | ├ (arm to the right/max) | jump home — a full period away |
| 5 | go to min | ┤ (arm to the left/min) | jump to the fire edge (fires next tick) |
| 6 | random | ✕ bowtie | jump to a random count |
| 7 | pole | diagonal split | jump to whichever end is nearer |
| 8 | stop | ▢ hollow box | freeze the counter until it's poked or reset |

The stored model is still `rule[source][dest]` (a value 0–7) — an 8×8 grid of
rule types, edited through this selector rather than as a raw matrix. A source
can drive several destinations (set each in turn). Default: all none, so out of
the box Meadow64 is eight independent clock-dividers until you wire rules. A
row may target itself (self-decrement = run faster), unusual but allowed.

**Stop & revive:** a stopped counter neither counts nor fires; any rule that
*moves* it (max / min / random / pole) revives it, as does RESET or re-tapping
its length on the play page.

**Cascade resolution per tick:** advance every counter once (the ones at min
fire); then process firings through a queue, applying their rules, where a
decrement-from-min enqueues the target to fire this tick — bounded by a
**fire-once-per-tick guard** per row so rule loops can't spin. Order within a
tick is row order (deterministic; the guard makes it musically order-insensitive).

## 4. Clock, reset, outputs

- Standard **clock divider** (÷1…÷64, the shared `P64::ClockDivider`, menu,
  serialized, ÷1 on reset).
- **RESET tick:** reload every counter to its home (`pos = L−1`) and re-zero
  the divider — the "bar 1" button.
- **Outputs:** 8 mono **T1–T8** (5 ms triggers) + a **POLY** 8-channel
  trigger out, post-mute. (Same I/O shape as Euclid64 / Bounce64.)

## 5. Menu, panel, serialization

- Menu: **clock divider**, and colours for **cursor**, **home marker**, **fire
  flash**, **mute**, the rules-page **glyph/selector** colour, and the
  **active/inactive page** colours. (The rule is read from its glyph, so it
  needs no per-action colour.)
- Panel: 10 HP, page grammar (orange), title **MDW64**, active-page light, 8
  mono jacks + poly (the Euclid64 column layout).
- Serialized: `L[8]`, the `rule[8][8]` matrix, `muted[8]`, clock divider,
  colours. Transient: `pos[8]`, sub-page, fire flashes.

## 6. Implementation stages (each a commit, panel first as usual)

1. **Core:** panel, countdown counters with length-set on the play page,
   cursor + home + flash LEDs, clock divider, mutes (scene buttons), T/POLY
   outputs, RESET, serialization.
2. **Rules:** the glyph rules page (top button 2) — source on scenes,
   destination on the left column, the 8 rule glyphs (6×6) and selection — and
   the queue-based cascade with the fire-once guard and stop/revive.
3. **Docs + release:** docs page, README entry, example patch (Meadow64 into
   8Notes for a generative melody?), version bump 2.14.0.

## Resolved decisions (2026-06-14)

1. **Counter model:** `pos` 0…L−1, fires at 0, reloads to L−1; length = period
   in ticks. Length set by tapping a column on the play page.
2. **Cross-rules via a meadowphysics glyph page** (revised 2026-06-14): scene
   = source, left column = destination, row = rule, a 6×6 glyph shows it. Eight
   rules — none, increment, decrement, go-to-max, go-to-min, random, pole,
   stop — stored as `rule[8][8]`.
3. **Decrement-from-min drives cascades**, bounded by a fire-once-per-tick guard.
4. **Mute gates the output only** — counters and their rules keep running.
5. **Page module**, scene mutes, 8 mono + poly trigger outs, standard clock
   divider (the Euclid64 / Bounce64 family shape).

## Appendix — glyph sketches (6×6, draft)

Drafts to refine and render at implementation; `#` = lit. The ball runs
leftward, so left = min/fire, right = max/home.

```
none        increment   decrement   go to max
......      ..##..      ......      ##....
.#....      ..##..      ......      ##....
..#...      ######      ######      ######
...#..      ######      ######      ######
....#.      ..##..      ......      ##....
......      ..##..      ......      ##....

go to min   random      pole        stop
....##      ##..##      ..####      ######
....##      ##..##      ..####      ######
######      ..##..      ##..##      ##..##
######      ..##..      ##..##      ##..##
....##      ##..##      ####..      ######
....##      ##..##      ####..      ######
```

none is a diagonal slash; increment / decrement are plus / minus; go-to-max /
go-to-min are brackets whose arm reaches toward the right (max/home) or left
(min/fire) edge; random is a bowtie; pole is a diagonal split; stop is a hollow
box.
