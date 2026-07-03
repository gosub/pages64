# 64Grains — design sketch

The third **kit companion** (after 64Drums and [64Objects](Objects64.md)):
**synthetic microsound** — Curtis Roads by way of a drum machine. Each cell
is a seeded recipe for a *micro-event cloud*: dust bursts, crackle clusters,
chirps, click trains, bubbles. Impulse-excited like its siblings, so it rides
the same 64-cell gate bus and reuses the kit shell (layout, quantize,
gated variety, seed contract) extracted in the 64Objects design.

Deliberately **not** granular sampling. Grain clouds over a sample buffer
would be a page module wanting sound-in-module — the second sanctioned
exception to the modularity boundary, which Acid64 is already queued up
asking for. Synthetic microsound needs no buffer, no exception, and no
competitor exists in the Rack library; the sampling variant stays a separate
roadmap fight.

## Engine

A trigger spawns a **cloud**: N grains scheduled over T milliseconds with a
density envelope. A grain is a windowed micro-burst (0.5–20 ms) of either a
sine (pitched families) or filtered noise (textural families). Per-cell
recipe draws:

- grain count, cloud length, density envelope (front-loaded / even / swelling)
- grain duration and window shape (gaussian soft … rectangular clicky)
- pitch center, per-grain scatter, and per-grain glide (glissons)
- filter color for noise grains; stereo scatter per grain

Cost is trivial — a grain is an oscillator and an envelope for a few
milliseconds. The pool schedules grains, not voices; hundreds are cheap.

## Families (row → texture, top → bottom, tunable)

| Row | Texture |
|---|---|
| 0 | dust — sparse single clicks |
| 1 | crackle — dense chaotic micro-pops |
| 2 | glitch — bit-reduced chirp fragments |
| 3 | chirp / glisson — pitch-swept sine grains |
| 4 | trainlet — pitched click trains |
| 5 | bubble — resonant pops, upward blips |
| 6 | hiss bursts — shaped noise grains |
| 7 | rumble — low, granular rolls |

Quantize applies to the pitched families (chirps, trainlets, bubbles):
*columns walk the scale* makes row 3 a melodic glisson keyboard. Landing
pitch rule as in 64Drums.

## Variety ingredients (per-cell gated)

- **Reverse** — cloud density envelope runs backward (swell into the hit)
- **Accelerando** — grain spacing tightens or relaxes across the cloud
- **Sweep** — the cloud pans across the field grain by grain
- **Glide** — per-grain pitch trajectories on otherwise static families

## I/O and panel

The kit-companion shell: four poly cell-gate inputs, stereo L/R mix, blue
accent, 64Drums panel layout with the new title.

## Versions

After 64Objects; next minor version when implementation starts. Name
alternative considered: 64Dust (rejected: describes one row, not the kit).
