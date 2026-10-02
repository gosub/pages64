# pages64 design principles

The decisions every module is measured against. New modules and features
either follow these or argue explicitly for an exception.

## Hands on the grid, away from the computer

pages64 exists to get you off the mouse and keyboard and onto a physical
instrument. The Launchpad *is* the instrument: page modules have no
interactive panel controls, and every performance gesture (holding pads,
chords, two-handed combinations, page switching) is designed for fingers on
hardware. The computer is where you patch and configure, not where you play.

That rules out designing for mouse play. **64Pads** mirrors the grid on screen
for seeing state, demos and the occasional click, but it is not a substitute
for the hardware and features are not shaped around it.

## Pages

- **Positional page identity is intentional.** Up to 16 pages per patch,
  arranged with a strong positional sense (monome sum style). No per-page
  colors in the page-select overlay; the two-color palette can't support it
  tastefully anyway.
- **Page switching stays a hardware gesture only.** No page-select CV input.

## Buttons

- **Button role convention** (details in CLAUDE.md): top round buttons 1–8
  carry static page configuration. 8 is page select, 6 is the global temp
  save/reload snapshot, 7 is reserved for the cross-page performance recorder
  (live looper or gesture recorder, one of the two; see ROADMAP.md). Scene
  buttons A–H are interactive play only.

## Modularity boundary

- Page modules emit gates, triggers and CV. Pitch mapping and voice allocation
  live in companion utility modules (64Notes, 8Notes), sound in companions
  (64Drums and the other kits). Sanctioned exception: Mlr64's built-in sample
  playback. Any new exception needs an argument as strong as mlr's.
- **Companion module naming and accent** (details in CLAUDE.md): companion
  modules reverse the name (64Notes, 8Notes, 64Pads, 64Drums) and swap the
  orange accent for the complementary blue `#22aff2`.

## Seeded randomness is a contract

Established by Rhythm64 and 64Drums: any generative module serializes its
seed, rerolls only on request, and *Initialize* returns the factory seed, so
patches always reload their music.

## Supported hardware

- pages64 targets grid controllers with an 8×8 grid **plus** a top row of
  buttons and a scene column (the Launchpad family, APC-style controllers),
  because the button-role convention needs both.
- **monome grids are out of scope.** The inspirations are monome apps and that
  audience owns grids, but the hardware mismatch is fatal, not cosmetic: a
  grid has no top round buttons and no scene column, so the button-role
  convention (page select on 8, snapshot on 6, recorder on 7, scenes as play
  surface) has no home. Every page module would need a per-device interaction
  redesign, not a codec.
- Device support is only written and shipped against hardware that is
  actually available to test on.
