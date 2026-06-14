# Meadow64 — design (pass 1)

A meadowphysics-style cascading-counter sequencer: eight countdown counters
whose firings reset and bump each other into evolving cross-rhythms. Version
target: 2.14.0. Status: **drafted 2026-06-14**, expanding the ROADMAP
milestone (the cross-rules become an 8×8 matrix — see §3).

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

Top button 2 opens an **8×8 rule matrix**. Cell `(r, c)` is the rule from
**source row r** to **target row c**: when row r fires, it applies that cell's
action to row c. Tapping a cell cycles the action; each is colour-coded:

- **off** (default) — no effect;
- **reset** — target jumps home (`pos = L−1`): a full period until its next
  fire;
- **increment** — target's count +1 (fires one tick later), clamped to home;
- **decrement** — target's count −1 (fires one tick sooner); if it was already
  at 0 the target **fires now too**, which is how cascades propagate.

A row may target several rows (several lit cells in its row) — this
generalizes the ROADMAP's single-target sketch and matches meadowphysics'
range rules. The diagonal (a row acting on itself) is allowed (e.g. self-
decrement = run faster) but unusual. Default: empty matrix, so out of the box
Meadow64 is just eight independent clock-dividers until you wire rules.

**Cascade resolution per tick:** advance every counter once (fire the ones
that reach 0); then process firings through a queue, applying their rules,
where a decrement-to-fire enqueues the target — with a **fire-once-per-tick
guard** per row so rule loops can't spin forever. Order within a tick is row
order; documented as such (the network is deterministic, not order-sensitive
in a musically meaningful way once the guard is in place).

## 4. Clock, reset, outputs

- Standard **clock divider** (÷1…÷64, the shared `P64::ClockDivider`, menu,
  serialized, ÷1 on reset).
- **RESET tick:** reload every counter to its home (`pos = L−1`) and re-zero
  the divider — the "bar 1" button.
- **Outputs:** 8 mono **T1–T8** (5 ms triggers) + a **POLY** 8-channel
  trigger out, post-mute. (Same I/O shape as Euclid64 / Bounce64.)

## 5. Menu, panel, serialization

- Menu: **clock divider**, and colours for **cursor**, **home marker**,
  **fire flash**, **mute**, and the three rule actions (**reset / increment /
  decrement**), plus the **active/inactive page** colours.
- Panel: 10 HP, page grammar (orange), title **MDW64**, active-page light, 8
  mono jacks + poly (the Euclid64 column layout).
- Serialized: `L[8]`, the `rule[8][8]` matrix, `muted[8]`, clock divider,
  colours. Transient: `pos[8]`, sub-page, fire flashes.

## 6. Implementation stages (each a commit, panel first as usual)

1. **Core:** panel, countdown counters with length-set on the play page,
   cursor + home + flash LEDs, clock divider, mutes (scene buttons), T/POLY
   outputs, RESET, serialization.
2. **Rules:** the 8×8 rule-matrix page (top button 2), the queue-based cascade
   with the fire-once guard, rule-action colours.
3. **Docs + release:** docs page, README entry, example patch (Meadow64 into
   8Notes for a generative melody?), version bump 2.14.0.

## Resolved decisions (2026-06-14)

1. **Counter model:** `pos` 0…L−1, fires at 0, reloads to L−1; length = period
   in ticks. Length set by tapping a column on the play page.
2. **Cross-rules as an 8×8 matrix** (source × target), action per cell cycling
   off/reset/increment/decrement — generalizes the ROADMAP's single target.
3. **Decrement-to-fire drives cascades**, bounded by a fire-once-per-tick guard.
4. **Mute gates the output only** — counters and their rules keep running.
5. **Page module**, scene mutes, 8 mono + poly trigger outs, standard clock
   divider (the Euclid64 / Bounce64 family shape).
