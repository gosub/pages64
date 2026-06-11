# Life64 — design (pass 1)

Conway's Game of Life as a page module. Version target: 2.10.0.
Status: **drafted 2026-06-11**, from the ROADMAP milestone description.

## Scope summary

The 8×8 grid is a Life colony. The Base64 clock steps the generations
(standard clock divider); pressing a pad toggles a cell at any time, so the
player seeds and perturbs the colony live. Scene buttons carry the performance
controls: freeze, clear, randomize, frame looping, a one-frame memory, and a
library of famous patterns. Outputs: 64 cell gates (the Gome64/Buttons64
format), row/column binary CVs, and a density CV.

---

## 1. Grid (play surface)

- **Live cell = lit pad** (cell color). The whole 8×8 grid is the colony;
  there is no control row.
- **Press a pad** → toggle that cell, frozen or running. Acting on press, not
  release — there are no two-button gestures on the play surface, and edit
  latency matters when punching cells into a running colony.
- **Generation step:** on each (divided) clock tick the standard B3/S23 rules
  are applied to the whole frame at once.
- **Edges:** bounded by default — cells outside the 8×8 are simply dead.
  A right-click **Wrap edges (torus)** option joins left↔right and top↔bottom,
  so gliders and spaceships orbit forever instead of dying on the wall.
  Serialized; default off.
- The colony keeps evolving while another page is active (the
  Euclid64/Bounce64 idiom: the page is an instrument, not a screen saver).

## 2. Scene buttons A–H (interactive play)

- **A = Freeze.** Toggle; LED lit while frozen. Clock ticks are ignored, the
  frame holds still, and the grid stays editable — the still-life drawing
  board. Unfreezing resumes evolution on the next tick.
- **B = Clear.** All cells die. Outputs drop accordingly.
- **C = Randomize.** Reroll the whole frame: each cell comes alive with
  probability *p*, from the right-click **Randomize density** menu
  {10 %, 20 %, 30 %} (default 20 %).
- **D = Loop.** Toggle frame looping; LED lit while looping. While looping,
  the module restores the *loop start frame* every *N* ticks, so the colony
  replays the same N-frame evolution forever (Life is deterministic: storing
  one frame replays the whole passage). Edits during a pass play out until the
  wrap discards them.
  - **Length:** hold D — the grid becomes a length display (pads 1…N lit,
    row-major, 1 to 64); tap a pad to set N. Release D to return to the
    colony. Default 16.
  - **The loop start frame is (re)captured by:** enabling the loop (current
    frame), unfreezing, randomizing, and the first cell drawn onto a cleared
    frame before the next tick (clear → draw → it loops from your drawing).
    Recalling memory (E) and loading from the library (G) also re-arm it —
    an addition beyond the original list, but the same intent: "this is the
    new material, loop from here."
- **E = Recall.** Redraw the memorized frame, replacing the colony. The
  memory slot is initialized with the **glider**, so E does something
  delightful on a fresh module.
- **F = Save.** Store the current frame into the memory slot (LED flash as
  confirmation). One slot, serialized with the patch.
- **G = Library.** Modal pattern browser (see §3). Tap G again while the
  browser is open to exit without loading.
- **H** unused for now.

## 3. Pattern library (the G modal)

While the browser is open the grid shows a selection page: each lit pad is a
famous pattern. Tap a pad → the pattern is loaded **centered** on the grid
(replacing the colony) and the browser closes. Tap G → close without loading.
The simulation keeps running underneath — outputs are unaffected by browsing;
only the display is borrowed (same philosophy as Mlr64's config pages).

Patterns are laid out by family, one row each, all with full-cycle bounding
boxes ≤ 8×8 (per [LifeWiki](https://conwaylife.com/wiki)):

| Row | Family | Pads |
|---|---|---|
| 1 | Still lifes | block 2×2, beehive 4×3, loaf 4×4, pond 4×4, boat 3×3, ship 3×3, tub 3×3, barge 4×4 |
| 2 | Oscillators | blinker (p2) 3×3, toad (p2) 4×4, beacon (p2) 4×4, clock (p2) 4×4, octagon 2 (p5) 8×8, figure eight (p8) 8×8 |
| 3 | Spaceships | glider 3×3, LWSS 5×4, MWSS 6×5, HWSS 7×5 |
| 4 | Methuselahs | R-pentomino 3×3, pi-heptomino 3×3, acorn 7×3, diehard 8×3 |

Rows 5–8 stay dark (room for future user slots). Notes:

- **Spaceships** want **wrap on**: on the torus they orbit forever (steady
  rhythmic output); bounded, they crash into the wall and decay into debris —
  also musical, differently.
- **Octagon 2** and **figure eight** fill the full 8×8, so with wrap *on*
  their edge cells interact across the seam and the oscillation mutates —
  intentional fun, not a bug to fix.
- **Methuselahs** can't run their famous thousand-generation careers in 64
  cells; here they are simply excellent chaotic seeds.
- Patterns ship as compile-time bitmaps (8 bytes each) in the module source;
  no RLE parsing needed.

## 4. Outputs

- **CELLS 1–4:** the Gome64/Buttons64 64-cell format, four 16-channel poly
  outputs (rows 1-2, 3-4, 5-6, 7-8). A live cell holds its channel at 10 V for
  as long as it lives (sustained gate). 64Notes' note-length modes make this
  musical either way: *gate follow* sustains, *fixed time* articulates births
  (a surviving cell does not retrigger — birth is the note-on).
- **ROWS:** 8-channel poly. Channel *r* reads row *r* as an 8-bit number,
  leftmost column = MSB, scaled `value / 255 × 10 V`.
- **COLS:** 8-channel poly. Channel *c* reads column *c* the same way,
  topmost row = MSB.
- **DENS:** mono CV, `liveCells / 64 × 10 V`.
- Right-click **Binary decode** switch: *classic* or *Gray code* — Gray
  decodes the bit pattern through the reflected-binary code first, which
  decorrelates a cell's grid position from the magnitude of its CV
  contribution and tends to turn small colony changes into smaller CV moves.
  Applies to ROWS and COLS; serialized; default classic.

## 5. Clock, reset, menu

- Standard **clock divider** (÷1…÷64, shared `P64::ClockDivider`, menu,
  serialized, reset to ÷1 in `onReset`).
- **RESET tick:** restarts the loop (jump to the loop start frame, counter
  re-zeroed) when looping; otherwise ignored — reset must never destroy a
  hand-drawn colony.
- **Right-click menu:** wrap edges, randomize density, binary decode, clock
  divider, **cell color** and **UI color** (the latter used by the loop-length
  display and the library page) via `P64::appendColorMenu`.

## 6. Panel

Page-module grammar (orange accent), title **LIFE64**. Seven jacks with badge
labels: CELLS 1–4, ROWS, COLS, DENS. No panel controls — everything is on the
Launchpad or in the menu.

## 7. Serialization

Saved: the 64-cell frame, memory frame, loop enabled + length, wrap, density,
decode mode, clock divider, colors. Transient (not saved): freeze state, loop
tick counter and start frame, library/length-display modal state — a reloaded
patch wakes up running from its saved frame.

## 8. Implementation stages (each a commit, panel first as usual)

1. **Core:** panel, cell toggling, B3/S23 stepping with clock divider, wrap
   option, CELLS outputs, LED display, serialization.
2. **Scene layer:** freeze, clear, randomize (+ density menu), save/recall
   (glider-initialized memory).
3. **Loop + library:** loop with hold-D length selection and start-frame
   re-arming, the pattern browser with the 22 built-ins, RESET-restarts-loop.
4. **Binary CVs:** ROWS/COLS/DENS outputs, Gray decode option, README docs
   page + example patch, version bump 2.10.0.

## Resolved decisions (2026-06-11)

1. **Edit while running:** always allowed; freeze is for *comfortable*
   editing, not the only editing.
2. **Loop start capture list** extended with memory recall and library load.
3. **Library browsing never interrupts the sound** (sim runs underneath).
4. **Default wrap off** (classic bounded Life; wrap is the option).
5. **Sustained cell gates** (no birth-trigger mode in v1; 64Notes' fixed-time
   mode already articulates births).
6. **RESET is loop-only** — it never clears the colony.
