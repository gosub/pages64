# test - offline checks

Standalone programs that exercise module code outside of Rack. They link
against `libRack` from the SDK, so `RACK_DIR` must point to it (the default
matches the local SDK; CI passes its own).

```
make -C test -j check      # build and run everything, nonzero on any failure
./test/persist_Buttons64   # or one module
```

## persist_&lt;Module&gt;

One binary per module, all built from `persistence.cpp` with the module's
source compiled in (`stubs.cpp` supplies the plugin globals as weak symbols,
so each binary needs only its own module). Checks, one CSV row each:

- **round_trip**: `dataToJson` → `dataFromJson` into a fresh module →
  `dataToJson` gives identical JSON. Patch save/load and the button-6 temp
  snapshot both go through this path.
- **format_key**: the saved data carries `"v"` = `P64::DATA_FORMAT`.
- **legacy_color**: the saved data with every `*Color` field re-encoded as a
  raw MkII velocity and `"v"` removed (the format before 2.23.0) loads back
  to the same colors.

New modules are picked up automatically from `src/*.cpp`.

## kit_golden_&lt;Kit&gt;

The kits' seed contract ("patches reload their music"), checked as sound.
Each kit's factory kit, and the same kit with every Variety toggle on,
renders each of its 64 cells alone (0.5 s at 48 kHz, fixed noise seed) and
must match `fixtures/<Kit>.txt`: per cell, left/right RMS, RMS of the first
difference, zero crossings and RMS of the second half, within 1e-3 relative.
A changed seed, recipe or voice fails it.

When a kit's sound changes **on purpose**, rewrite the fixtures and commit
them with the change, so the break is deliberate and visible in review:

```
make -C test fixtures
```

`rack_host.cpp` gives these binaries an app context with an engine (the kits
read its sample rate); it includes Rack's headers individually because the
engine setup is off limits to plugins through `rack.hpp`.
