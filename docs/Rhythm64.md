# Rhythm64 (titled RTM64)

*Part of [pages64](../README.md).*

This module makes every pad the owner of a **fixed random rhythm**: hold a pad
and its rhythm plays on the Base64 clock; release it and it stops. The grid
becomes a mixing desk of 64 pre-composed parts — a generative drum machine
when paired with **[64Drums](64Drums.md)**, or a generative melody engine
through **[64Notes](64Notes.md)**.

The rhythms are not arbitrary noise; the grid has a shape:

- **Rows set density and feel.** Top rows are busy (hat territory, up to
  ~12 hits per 16 steps); bottom rows are sparse (kick territory, 2–4 hits)
  and increasingly **biased onto the strong beats**, so low parts land on the
  grid of the bar while high parts skitter.
- **Columns are siblings**: same density, different placement. Slide along a
  row to audition variations of the same part.

All 64 patterns run on one shared step counter (**8 / 16 / 32 steps** in the
menu, default 16), advanced by the divided clock; RESET restarts the bar. A
pad flashes bright on each of its hits.

**Latch (scene A):** tap A to switch to latch mode — taps arm and disarm pads
and they keep playing while you visit other pages. Tapping A again turns
latch off and silences everything. In momentary mode (default), leaving the
page releases all pads.

**Punch-in FX (scene B):** hold B and the grid becomes a Pocket
Operator-style effect selector — **rows are effects, columns the amount**,
one press picks both. The effect runs while B *and* a pad are held (last
press wins) and vanishes on release:

| Row | Effect | Columns |
|---|---|---|
| 1 | Loop | roll the last 1 … 16 steps (leftmost = repeat the step) |
| 2 | Ratchet | every hit becomes ×2 … ×24 sub-hits |
| 3 | Time | ÷4, ÷3, ÷2 · reverse · ×2, ×3, ×4, ×8 |
| 4 | Density | thin ← → fill |
| 5 | Mask | kicks only ← → hats only |
| 6 | Shuffle | reorder time slices, window 2 → whole loop |
| 7 | Push/drag | hits early ← → late |
| 8 | *(reserved)* | — |

Everything is non-destructive: the step counter keeps running in global time
underneath, so releasing B drops you back exactly where the band is. Nothing
latches and nothing is saved — punch-in is a performance gesture. Design
rationale: [PunchIn64.md](design/PunchIn64.md).

**Reroll (right-click menu):** draws a new seed — 64 brand-new rhythms. The
seed is saved with the patch, so a patch always reloads *its* rhythms;
randomness happens when you ask for it, never at load time. *Initialize*
returns the factory seed.

**Outputs:** the 64-cell trigger format — four 16-channel poly jacks (rows
1-2, 3-4, 5-6, 7-8), 5 ms triggers, ready for 64Drums or 64Notes patched 1:1.

The menu also has the standard **clock divider** (÷1 … ÷64) and the **armed**
/ **hit** colors. Design rationale: [Drums64.md](design/Drums64.md).
