# Inertia64 (titled INRT64)

*Part of [pages64](../README.md).*

This module turns each of the eight columns into a spinning disc with mass
and inertia — a zero-friction flywheel. Once set spinning, a disc spins
forever (even while another page is active); pedals are the only way to
change its speed:

- **Throttle (rows 1–4):** hold a pad to accelerate the disc. The top row is
  the hardest throttle; row 4 the gentlest.
- **Brake (rows 5–8):** hold a pad for friction. Row 5 is the gentlest
  brake; the bottom row the hardest.
- Intensity grows toward the grid edges, the gentle pedals meet in the
  middle. Pedals are momentary; the strongest held pedal per side applies,
  and riding gas and brake together is allowed — the net torque is their
  difference.
- **Handbrake (scene buttons A–H):** tap to stop column 1–8 instantly; the
  disc freezes where it stands. Scene LEDs are lit while their disc spins,
  so the right column reads as a speed overview.

The column's light cursor is a fixed point on the disc rim seen edge-on: it
rides up and down sinusoidally, sweeping fast through the middle and slowing
into the turnarounds — that *is* the disc, and it reads as speed at a
glance. Held pedals light up in the pedal color.

**Outputs:** **X** is an 8-channel poly output carrying ±5 V sines, one per
column — the X coordinate of each rim point, so the frequency *is* the disc
speed: silky LFOs that you push and drag rather than set. **VEL** carries
each disc's angular velocity as 0–10 V. A RESET tick stops and re-zeros all
discs.

In the right-click menu you can set the **max speed** (4, 8, 16 or 32
revolutions per second — from slow-sweep to audio-rate-adjacent; the VEL
scale follows), and choose the **cursor color** and **pedal color**. Disc
speeds and positions are saved with the patch.
