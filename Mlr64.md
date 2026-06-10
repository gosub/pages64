# Mlr64 — design (pass 1)

Page module with built-in sample playback, after [mlr](https://llllllll.co/t/mlr/)
by tehn. Version target: 2.8.0. Status: **locked 2026-06-10** (decisions at
the bottom resolved with Giampaolo).

## Scope summary

8 sample lanes on the 8 grid rows, 8 slices per lane, press = jump, two-button =
sub-loop. Varispeed sync to the Base64 clock (no timestretch, no offline
resampling). Scene buttons: 4 group mutes + 4 pattern recorders. Top buttons:
play surface + two config sub-pages. Audio: stereo mix out + 8-channel poly
lane out.

---

## 1. Grid (play page)

- **Rows 1–8 = lanes** (top to bottom), one loaded loop each, divided into
  **8 equal slices** (columns).
- **Press a pad** → that lane's playhead jumps to the start of slice `col`,
  quantized to the *quantize* setting. Playback always continues from there
  (lanes loop by default).
- **Hold two pads in one lane** → sub-loop spanning the two slices (inclusive).
  The lane loops inside that region until a single press resets it: a single
  press anywhere in the lane returns to the full loop and jumps to that slice
  (mlr behavior).
- **LED feedback per lane:** loaded region dim (sub-loop region only, when one
  is active), playhead bright, empty lane dark. One LED frame per tick is
  enough; no per-sample LED updates.
- Single presses act on **press** (not release) — latency matters more than
  gesture disambiguation here; the two-button gesture is detected when the
  second pad goes down while the first is held (same idiom as Flin64's length
  gesture, but acting on press).

## 2. Scene buttons A–H (interactive play)

- **A–D = group stops 1–4 (mlr-style).** Each lane is assigned to a group
  (config page). Pressing the scene button **stops** the group's lanes:
  playheads halt, the lane rows go dim. Pressing again restarts every lane in
  the group **from its loop start, quantized** like any jump — so a group
  re-entry lands on the grid. (Chosen over the keep-running mute used by
  Euclid64/Bounce64: the stop-and-drop is part of the original's chop feel.)
  LED: green = group playing (has lanes), red = stopped, off = no lanes
  assigned. Pressing a pad in a stopped lane also restarts just that lane at
  the pressed slice.
- **E–H = pattern recorders 1–4.** State machine per recorder:
  - *empty* → tap: **arm**; recording starts at the next quantize point; grid
    presses on the play page are captured with tick timestamps.
  - *recording* → tap: close the loop (length rounded up to the next beat) and
    start looping playback of the recorded jumps.
  - *playing* → tap: mute/unmute the recorder. **Hold (> 1 s): clear.**
  - LED: off = empty, red = recording, green = playing, dim = muted.
  - v1 records **jump gestures only** (lane, slice, tick offset); sub-loop
    gestures are not recorded.
  - These are page-local recorders; the global cross-page gesture recorder
    (top button 7, see ROADMAP) remains a separate future feature.

## 3. Top buttons 1–5 (static config)

- **1 = Play page** (default).
- **2 = Lane config page:** rows = lanes. Columns 1–4: group assignment
  (radio); column 6: loop mode; column 7: one-shot mode (radio pair).
  A one-shot lane sits silent until pressed, plays from the pressed slice to
  the end of the (sub-)loop, then stops.
- **3 = Beats page:** rows = lanes. Columns 1–8 set beats-per-loop from
  `{1, 2, 4, 8, 16, 32, 48, 64}` (radio, bar display like Cafe64's length
  editor).
- 4–5 unused for now (6–8 reserved globally).

## 4. Sync (the core decision: varispeed, measured clock)

- **Tempo source:** Base64 clock ticks (`LeftMessage::clockTick`). The module
  measures the tick period in engine frames and smooths it (median of the last
  3 intervals, then 1-pole). A *ticks per beat* menu `{1, 2, 4, 24}` (default
  **1** = clock carries quarter notes) converts tick period → seconds per beat.
- **Playback increment** per lane:
  `inc = srcFrames / (beats × secPerBeat × engineRate)` source-frames per
  engine frame. Tempo mismatch becomes pitch shift (varispeed); sample-rate
  conversion falls out of the same equation. Linear interpolation read.
- **No clock:** free-run at the last measured tempo (120 BPM before any tick).
- **Reset input:** re-zeros every lane to its loop start and restarts the
  pattern recorders — the "bar 1" button.
- **Phase model: loose.** Lanes are *not* hard-resynced every loop; varispeed
  keeps them locked if they started together (use RESET to line everything
  up). A "hard re-sync at loop boundary" option can be added later if drift in
  long sets proves real.
- Standard **clock divider** is *not* needed (tempo is measured, not stepped),
  but **quantize** is: menu `{off, 1 tick, 2 ticks, 1 beat}` (default 1 tick)
  governs jump latency and recorder timing.

## 5. Samples

- **Loading:** right-click menu, one entry per lane ("Lane N: load sample…",
  shows the current filename, plus "clear"). WAV only in v1, decoded with a
  vendored `dr_wav.h`. File drop on the panel loads into the lane under the
  drop point (fallback: first empty lane).
- **Beats per loop guess** at load, in priority order:
  1. filename contains `<N>bpm` → `beats = round(duration × N / 60)`;
  2. clock running → `beats = duration / secPerBeat` snapped to
     `{1,2,4,8,16,32,48,64}`;
  3. fallback: assume 120 BPM, snap as above.
  Always correctable on the Beats page.
- **Stereo** files keep stereo into the mix outs; the poly lane out carries
  `(L+R)/2`. Mono files play centered.
- **Prep philosophy:** cleanly cut loops are the user's job, as in mlr.
  Varispeed handles tempo mismatch; pitch shift is the aesthetic.
- **Threading:** decode on the UI thread into a fresh buffer, publish to the
  engine via an atomic slot swap (engine never blocks; a lane mid-swap renders
  silence for one block). Patch load (`dataFromJson`) re-loads from stored
  absolute paths synchronously.

## 6. Audio path & I/O

- Per lane: buffer, playhead (double), sub-loop bounds, group, beats, mode.
- **Declick:** every jump crossfades old→new position over ~2 ms (dual read
  during the fade). Group stop/restart ramps over the same window.
- **Outputs:** `MIX L`, `MIX R`, `POLY` (8 channels, one per lane, post-mute).
  No inputs — clock and reset arrive through the expander chain.
- No per-lane volume in v1 (patch the poly out into a mixer for that).

## 7. Panel

10 HP, page-module grammar (orange accent — it *is* a page), title **MLR64**.
Three jacks: POLY, L, R (badges). The body stays mostly empty in v1; lane
filename display is a possible later addition (NanoVG text, not SVG).

## 8. Serialization

Per lane: absolute sample path, beats, group, mode, sub-loop. Module: ticks
per beat, quantize, group stop states. Transient (not saved): playheads,
recorder contents, recorder states.

## 9. Implementation stages (each a commit, panel first as usual)

1. **Core:** lane struct, dr_wav loading via menu, varispeed loop playback,
   tempo measurement, jump-on-press with quantize, playhead LEDs, mix + poly
   outs, serialization of lane config.
2. **Performance layer:** two-button sub-loops, groups + scene mutes, config
   sub-pages (groups/mode, beats), one-shot mode, declick crossfades.
3. **Pattern recorders** (scenes E–H), file-drop loading, README + example
   patch, version bump 2.8.0.

## Resolved decisions (2026-06-10)

1. **Scene split:** A–D group stops, E–H pattern recorders (4/4).
2. **Mute semantics:** mlr-style **stop** — halt on stop, restart from loop
   start (quantized) on re-press.
3. **Slice count:** fixed at 8 in v1.
4. **Ticks per beat:** default **1** (clock carries quarter notes).
5. **Recorder loop length:** elapsed time **rounded up to the next beat**.
