# Inertia64 — design (pass 1)

Eight spinning discs with mass and inertia, played with accelerator and brake
pedals. Version target: 2.12.0. Status: **drafted 2026-06-12**, from the
ROADMAP milestone description.

## Scope summary

Each column is a flywheel. The top four pads of the column are throttle
pedals, the bottom four are brake pedals, each row a different intensity;
both act only while held. Per column the module outputs the X coordinate of a
fixed point on the disc rim (a sine whose frequency is the disc speed) and
the angular velocity. Two 8-channel poly outputs (X, VEL). No clock — the
physics is continuous. Title **INRT64**.

## 1. Physics

- Per column: phase `φ` (revolutions, wraps 0–1) and angular velocity `ω`
  (revolutions per second). Integrated per sample: `φ += ω · dt`.
- **Idealized flywheel: zero passive friction.** A spinning disc spins
  forever (the Flin64 spirit — set it going, play something else); brakes
  are the only way to slow down. Discs keep spinning while another page is
  active.
- **ω is clamped to [0, ωmax]** — no reverse; braking stops at standstill.
  `ωmax` from the *Max speed* menu {4, 8, 16, 32} rev/s, default **16**
  (LFO territory at the top, slow sweeps at the bottom).

## 2. Pedals (the grid)

- **Rows 1–4 = throttle, rows 5–8 = brake**, intensity grows toward the
  edges (top row = hardest throttle, bottom row = hardest brake; the middle
  rows are the gentle pedals). Rates, in rev/s²:

  | Row | Pedal | Rate |
  |---|---|---|
  | 1 | throttle | 8 |
  | 2 | throttle | 4 |
  | 3 | throttle | 2 |
  | 4 | throttle | 1 |
  | 5 | brake | 1 |
  | 6 | brake | 2 |
  | 7 | brake | 4 |
  | 8 | brake | 8 |

  With ωmax 16: full throttle spins up in 2 s, the gentlest pedal takes 16 s.
- **Momentary only:** a pedal acts while held, on press/release. Several
  pedals held in one column: the strongest held throttle and the strongest
  held brake both apply (riding gas and brake is allowed and useful — net
  torque is their difference).
- Held pedal pads are lit in the pedal color.

## 3. Scene buttons — handbrakes

Scene A–H: **tap = instant stop** of column 1–8 (`ω = 0`; the phase stays
where it froze). The panic/punctuation gesture — eight discs take a while to
brake by pedal. Scene LED lit while the column is spinning (a speed overview
at the grid's edge).

## 4. Display

The column's cursor is the rim point seen edge-on: `x = sin(2πφ)` maps to
the 8 rows (top = +1). It rides sinusoidally — fast through the middle,
slowing into the turnaround at the edges — which *is* the disc, and reads as
speed at a glance. Cursor in the cursor color; held pedals overlay in the
pedal color.

## 5. Outputs, reset, menu

- **X** (poly 8ch): `5 V · sin(2πφ)` per column — bipolar ±5 V, the
  LFO convention; frequency = the disc speed.
- **VEL** (poly 8ch): `10 V · ω / ωmax`, unipolar.
- **RESET tick:** all discs stop and re-zero (`ω = 0, φ = 0`).
- No clock divider — nothing is clocked.
- Menu: **Max speed** {4, 8, 16, 32} rev/s, **cursor color**, **pedal
  color**.
- Serialized: `ω[8]`, `φ[8]`, max-speed index, colors. Transient: held
  pedals.

## 6. Panel

10 HP, page grammar, title **INRT64**. Two badged jacks: X, VEL.

## 7. Implementation stages (each a commit, panel first as usual)

1. **Core:** panel, per-sample physics, pedal handling, X/VEL outputs,
   cursor + pedal LEDs, serialization.
2. **Polish:** scene handbrakes + spinning LEDs, max-speed menu, docs page,
   README entry, version bump 2.12.0.

## Resolved decisions (2026-06-12)

1. **Zero passive friction** — flywheels, not turntables.
2. **No reverse** — ω clamps at 0.
3. **Pedal intensity grows toward the grid edges**, gentle pedals in the
   middle.
4. **Strongest-held-pedal wins per side**, gas and brake may overlap.
5. **X is bipolar ±5 V**, VEL unipolar 0–10 V.
6. **Scene buttons are per-column handbrakes**, lit while spinning.
