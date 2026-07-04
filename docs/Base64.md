# Base64 (titled BASE64)

*Part of [pages64](../README.md).*

This is the central module. Once loaded, you can select the Launchpad MIDI
interface and attach the "page" modules to its right. The LEDs at the top
indicate the number of modules connected and the currently active module (green)
and inactive ones (yellow). The output jacks at the bottom provide a CV signal
for the currently active page (0V for the first page, 1V for the second, and so
on) and a trigger signal when a page is changed.

To switch to a different page, keep pressed the rightmost button in the top round
button row of the Launchpad (labeled 8), and press a button from the top row of
the grid; each lighted button is a page.

<p align="center">
  <img src="img/base64_pages.svg" width="540" alt="Holding button 8 lights one top-row pad per connected page; the active page is green, the others yellow"><br>
  <em>Page selection: hold button 8 and press a lit top-row pad.</em>
</p>

Across all page modules the Launchpad's extra buttons follow one convention: the
**top round buttons (1–8)** carry static page configuration (button 8 is always
page select; button 6 is the global snapshot below; 7 is reserved for the
cross-page gesture recorder), while the **scene buttons (A–H)** on the right are
for interactive play — latch modes, mute groups and the like.

## Global key

The right-click menu sets a **global key** — root note and scale — that the
pitched modules (Keys64, 64Notes, 8Notes) follow by default, so the whole
instrument changes key from one place. Each follower has a *Follow Base64
global key* switch in its menu; picking a local scale or root on a follower
turns its follow off (an override), and re-enabling the switch snaps it back
to the global key. The key is saved with the patch (Base64 owns it).

## Swing

Base64 distributes the clock to the whole chain (feed CLK a **straight**
clock, typically ×4 your base tempo, and let each module's clock divider
subdivide), so it is also where the groove lives. The right-click **Swing**
menu sets the **amount** (off/50% … 75%, 66% = triplet feel) and the
**unit** — whether every 2nd *tick* swings (16th swing when you clock ×4) or
every 2nd *pair* (8th swing at ×4).

Base64 measures the incoming tick period and delays the odd ticks of the
broadcast clock by the swing amount; the RESET input re-zeros the swing
phase along with everything else, and because clock dividers count from the
downbeat, modules on even divisions (÷2, ÷4 …) stay straight while
tick-level modules swing — like a classic drum machine, quarters and 8ths
don't move. The measured period is also shared with the chain, so Mlr64's
varispeed stays stable under swing. Send RESET after repatching to line the
groove up.

## Temp save / temp reload (button 6)

The Elektron trick, for the whole chain: **hold button 6** for about a second —
it flashes green — and every page module (active or not) snapshots its state,
along with the active page. Mangle everything live; **tap button 6** to snap
back to the saved state. The snapshot lives in memory only: it is not stored
in the patch, and reloading before any save does nothing.
