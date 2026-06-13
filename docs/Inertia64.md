# Inertia64 (titled INRT64)

*Part of [pages64](../README.md).*

This module turns each of the eight columns into a mass with momentum that
accelerates *upward*. Its position wraps modulo the grid — when the cursor
leaves the top it reappears at the bottom — so the column reads as a rising
sawtooth whose speed you set by pedaling. Once moving, a mass keeps moving
forever (even while another page is active); pedals are the only way to
change its speed:

- **Throttle (rows 1–4):** hold a pad to accelerate the mass. The top row is
  the hardest throttle; row 4 the gentlest.
- **Brake (rows 5–8):** hold a pad for friction. Row 5 is the gentlest
  brake; the bottom row the hardest.
- Intensity grows toward the grid edges, the gentle pedals meet in the
  middle. The pedals are deliberately heavy — a quick tap of a gentle pedal
  is a small nudge; speed builds as you hold. They are momentary; the
  strongest held pedal per side applies, and riding gas and brake together
  is allowed (the net is their difference).
- **Handbrake (scene buttons A–H):** tap to stop column 1–8 instantly; the
  mass freezes where it stands. Scene LEDs are lit while their mass is
  moving, so the right column reads as a speed overview.

The column's lit pad is the mass itself, climbing and wrapping; the faster it
moves the faster it climbs.

**Outputs:** **POS** is an 8-channel poly output carrying each column's
position as a 0–10 V rising ramp (bottom = 0 V, top = 10 V) — a per-column
phasor / sawtooth LFO you push and drag rather than set. **VEL** carries each
mass's speed as 0–10 V. A RESET tick stops and re-zeros all masses.

POS is **declicked** by default: the output is gently slewed (about 1 ms for
a full swing) so the sawtooth's wrap from top back to bottom — and the jump
when you send a column home or reset — soften into a click-free edge instead
of a hard step. The slew is far faster than the rising ramp, so it only
rounds the discontinuities; turn it off in the menu if you want the raw step.

In the right-click menu you can set the **max speed** (1, 2, 4 or 8
traversals per second — the POS sawtooth's top frequency, from slow drift to
a brisk LFO; the VEL scale follows), toggle **declick POS output**, and
choose the **cursor color** and **pedal color**. Mass speeds and positions
are saved with the patch.
