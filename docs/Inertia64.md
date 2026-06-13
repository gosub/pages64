# Inertia64 (titled INRT64)

*Part of [pages64](../README.md).*

This module turns each of the eight columns into a mass with momentum. Its
position wraps modulo the grid — when the cursor leaves the top it reappears
at the bottom (and vice versa) — so the column reads as a sawtooth whose
speed you set by pedaling. Once moving, a mass keeps moving forever (even
while another page is active); pedals are the only way to change its speed:

- **Up pedals (rows 1–4):** hold a pad to accelerate the mass upward. The top
  row is the hardest; row 4 the gentlest.
- **Down pedals (rows 5–8):** hold a pad to push downward. Row 5 is the
  gentlest; the bottom row the hardest. On a *monodirectional* lane the down
  pads only brake the mass to a stop; on a *bidirectional* lane they keep
  pushing past zero into reverse (set the mode on the Direction page below).
- Intensity grows toward the grid edges, the gentle pedals meet in the
  middle. The pedals are deliberately heavy — a quick tap of a gentle pedal
  is a small nudge; speed builds as you hold. They are momentary; the
  strongest held pedal per side applies, and pushing both ways at once is
  allowed (the net is their difference).
- **Handbrake / home (scene buttons A–H):** tap a moving column to stop it
  instantly — the mass freezes where it stands. Tap it again, now stopped,
  to send it home to the bottom. Scene LEDs are lit while their mass is
  moving (a speed overview at the right column) and off when stopped, so the
  LED also tells you which action the next tap will take.

The column's lit pad is the mass itself, climbing (or falling) and wrapping;
the faster it moves the faster it travels.

By default a moving mass never stops — but give a lane some **friction** and it
behaves like a flywheel in air: a held pedal settles at a steady cruising speed
(each pedal a different speed), and releasing it lets the mass coast to a stop.
More friction means a lower cruise and a quicker stop.

**Pages (top buttons):**

- **1 — Play** (default): the pedals.
- **2 — Direction:** tap anywhere in a column to toggle it between
  monodirectional and bidirectional. A column shows its mode with its top cell
  lit for up-only, and both its top and bottom cells lit for both-ways.
- **3 — Friction:** a bar per column sets that lane's friction. Tap a row to
  set the amount to its height (bottom = a little, top = a lot); tap the top of
  the bar again to clear it back to none. An empty column is a frictionless
  flywheel.

Leaving the play page releases any held pedals, and the masses keep moving on
every page.

**Outputs:** **POS** is an 8-channel poly output carrying each column's
position as a 0–10 V ramp (bottom = 0 V, top = 10 V) — a per-column phasor /
sawtooth LFO you push and drag rather than set. **VEL** carries each mass's
signed speed: a monodirectional lane reads 0–10 V, a bidirectional one ±10 V.
A RESET tick stops and re-zeros all masses.

POS is **declicked** by default: the output is gently slewed (about 1 ms for
a full swing) so the sawtooth's wrap from top back to bottom — and the jump
when you send a column home or reset — soften into a click-free edge instead
of a hard step. The slew is far faster than the rising ramp, so it only
rounds the discontinuities; turn it off in the menu if you want the raw step.

In the right-click menu you can set the **max speed** (1, 2, 4 or 8
traversals per second — the POS sawtooth's top frequency, from slow drift to
a brisk LFO; the VEL scale follows), toggle **declick POS output**, and
choose the **cursor**, **pedal** and **page** colors. Mass speeds and positions
are saved with the patch.
