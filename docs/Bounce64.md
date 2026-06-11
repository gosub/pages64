# Bounce64 (titled BNCE64)

*Part of [pages64](../README.md).*

This module is a bouncing-ball rhythm machine, after the *Tenori-on*'s bounce
mode (and monome's *boiingg*). Each column holds one ball: press a pad in rows
A–G to drop it from that height. The ball falls one row per clock tick, fires a
trigger when it hits the floor, and climbs back up to its apex — so a ball
dropped from height *h* triggers every *2·h* ticks. Higher drop = slower pulse.

- **Drop / re-drop:** press rows A (height 7) through G (height 1). Pressing a
  new height while the ball is bouncing re-drops it immediately, so you can
  dribble a column in time.
- **Remove:** press the bottom row (H).
- **Mute:** scene buttons A–H mute columns 1–8; a muted ball keeps bouncing in
  the mute color, so it re-enters in phase.

A dim apex marker shows each ball's drop height; the ball flashes the hit color
on the floor. Balls keep bouncing while another page is active. Different drop
heights interlock into polyrhythms the same way Flin64's rays do, but with a
bouncier feel — the two pair well in one chain.

The module provides 8 mono trigger outputs (T1–T8) and a polyphonic output
carrying all 8 triggers. In the right-click menu you can select a **clock
divider** (÷1 through ÷64) and colors for the **ball**, **apex marker**,
**floor hit**, and **mutes**.
