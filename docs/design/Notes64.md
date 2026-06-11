● Here's the concrete build plan for Notes64 — the standalone note-mapper that turns the 64
  cell gates into pitched polyphony.

  Shape of the module

  Unlike the page modules, Notes64 is a plain rack::Module (no Launchpad chain, no PageModule).
  It's pure CV in → CV out.

  I/O
  enum InputIds  { ENUMS(CELL_INPUT, 4), TRANSPOSE_INPUT, CLOCK_INPUT, NUM_INPUTS };
  enum OutputIds { PITCH_OUTPUT, GATE_OUTPUT, NUM_OUTPUTS };   // both poly, maxPoly channels
  - 4 poly gate inputs (16ch each) = 64 cells, identical layout to Gome64/Buttons64 so they
  patch 1:1. Reconstruct the grid from (in, ch): row = 2*in + ch/8, col = ch%8, cell = row*8 +
  col.
  - TRANSPOSE_INPUT (1V/oct, added to everything) and an optional CLOCK_INPUT (for clock-synced
  note length).

  Cell → pitch map

  A rebuildNoteMap() that fills int noteMap[64] (MIDI numbers), recomputed whenever config
  changes. Scale math lives in a shared new src/Scales.hpp:
  struct Scale { const char* name; int size; int deg[12]; };  // Major = {0,2,4,5,7,9,11}
  int degreeToSemitone(const Scale&, int d);  // handles octave wrap for d<0 and d>=size
  The four arrangements, as concrete formulas (base = rootMidi + 12*octave):

  ┌───────────────────┬────────────────────────────────────────────────────────────────────┐
  │       Mode        │                    Formula for cell (row, col)                     │
  ├───────────────────┼────────────────────────────────────────────────────────────────────┤
  │ 1D linear         │ base + degreeToSemitone(scale, row*8 + col) — pitch rises top-left │
  │                   │  → bottom-right                                                    │
  ├───────────────────┼────────────────────────────────────────────────────────────────────┤
  │ 2D isomorphic     │ base + col*colInterval + row*rowInterval (semitones; e.g. 2        │
  │                   │ across, 5 down)                                                    │
  ├───────────────────┼────────────────────────────────────────────────────────────────────┤
  │ 2D scale grid     │ base + degreeToSemitone(scale, col + rowDegrees*row) — horizontal  │
  │ (gome-native)     │ = scale degrees, vertical = a diatonic fourth (rowDegrees=3), so   │
  │                   │ everything stays in-key                                            │
  ├───────────────────┼────────────────────────────────────────────────────────────────────┤
  │ Chord per cell    │ cell → root degree d; chord = {d, d+2, d+4(, d+6)} through the     │
  │                   │ scale → diatonic triad/7th, spread across voices                   │
  └───────────────────┴────────────────────────────────────────────────────────────────────┘

  Voice allocator (the core)

  struct Voice { bool active; int cell; float pitch; int64_t age; };
  Voice voices[16];
  bool  cellGate[64];   // previous frame, for edge detection
  Each frame: read all 64 gates, edge-detect against cellGate[].
  - Rising edge on cell c → compute note(s), allocate. Free voice if any; else steal per
  strategy; brief 1-sample gate dip on the chosen voice so envelopes retrigger.
  - Falling edge → release every voice with cell == c (so chord voices die together).

  Stealing strategies (menu): oldest (min age), newest (max age), lowest/highest (min/max
  pitch), round-robin (rr++ % maxPoly), no-steal (drop new, with same-pitch fold). maxPoly 1–16
  sets setChannels().

  Output is trivial: PITCH.setVoltage((midi-60)/12 + transpose, v),
  GATE.setVoltage(active?10:0, v).

  The one thing that needs your call: note length

  Gome64 emits 5 ms triggers, so naive gate-follow would give 5 ms clicks. Notes64 therefore
  needs a hold mechanism. I'd implement three modes (menu): fixed-time (ms), clock-synced (via
  CLOCK_INPUT), and gate-follow (for sustained sources like Buttons64 toggles). Default =
  fixed-time, so Gome64 sounds musical out of the box.

  The rest

  - Serialization: arrangement, scale, root, octave, intervals, rowDegrees, chordType, maxPoly,
  stealStrategy, noteLengthMode, fixedMs. rebuildNoteMap() on load.
  - Panel: ~12HP, Cafe64 grammar; 4 input jacks (plain white labels per the input convention) +
  PITCH/GATE outputs (white badges). Title via gen_title_paths.py.
  - Registration: extern modelNotes64 in plugin.hpp, addModel in plugin.cpp, plugin.json entry,
  version bump 2.4.0 → 2.5.0.

  Before I build, three quick decisions

  1. Note length default — fixed-time ms (recommended, works with Gome64 triggers), or
  clock-synced as the headline mode?
  2. Velocity output — skip it for v1 (sources are binary gates), or add a poly VELOCITY out at
  a fixed level?
  3. Config surface — all in the right-click menu (matches the rest of the plugin), or expose a
  few knobs (root, octave, max poly) on the panel?

  Want me to drop into plan mode and write this up properly, or just start building once you've
  answered those three?