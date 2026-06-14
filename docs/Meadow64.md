# Meadow64 (titled MDW64)

*Part of [pages64](../README.md).*

This module is a cascading-counter sequencer after monome's *meadowphysics*.
Each of the eight grid rows is a countdown counter: on every clock tick its
lit pad steps one column toward the left, and when it reaches the left edge
the row fires a trigger and the pad jumps back to its start. The magic is that
a firing counter can also **reach over and change the others** — and those
knock-ons cascade — so a handful of plain counters grow into intricate,
self-evolving cross-rhythms.

**Play page (top button 1):**

- **Set a row's speed:** tap a pad in that row. The column is the length, i.e.
  the period — column 1 fires every tick, column 8 every eight ticks. The pad
  jumps to that column (its home) and counts down from there.
- The moving **cursor** is bright; a dim **home marker** shows where it
  reloads, so you can read the length even while it travels; the row flashes
  when it fires.
- **Mute (scene buttons A–H):** mute rows 1–8. A muted counter keeps counting
  and keeps driving its cross-rules — only its trigger *output* is silenced —
  so it stays in phase.

**Rules page (top button 2)** — this is the heart, a meadowphysics-style glyph
selector. You set one *source → destination* rule at a time:

- **Scene buttons A–H pick the source row** (whose firing triggers the rule).
- **The leftmost grid column picks the destination row.** The chosen
  destination is shown as a dim horizontal line across the grid; in the left
  column, destinations that already carry a rule from this source glow dim.
- **The rest of the grid picks the rule** — press any pad and its *row*
  chooses one of the eight rules below; the choice is drawn as a glyph in the
  centre of the grid.

The eight rules, applied to the destination counter when the source fires:

| Rule | Effect |
|---|---|
| none | nothing (default) |
| increment | nudge it one step *later* |
| decrement | one step *sooner* — and if it's at the edge it fires now, cascading |
| go to max | send it home (a full period away) |
| go to min | send it to the fire edge |
| random | jump it to a random point |
| pole | jump it to whichever end is nearer |
| stop | freeze it until something moves it (or RESET) |

A source can drive several destinations; the whole thing is an 8×8 web of
rules. Out of the box the web is empty, so Meadow64 is just eight independent
clock-dividers until you start wiring rules — then it comes alive.

**Outputs:** 8 mono **T1–T8** (5 ms triggers) and a **POLY** 8-channel trigger
out (post-mute) — feed the poly into **8Notes** for an instant generative
melody, or the mono outs into drums. A RESET tick sends every counter home and
revives any that were stopped.

In the right-click menu you can select a **clock divider** (÷1 through ÷64) and
colors for the **cursor**, **home marker**, **fire flash**, **mute**, the rules
**UI**, and the rules **destination line**.
