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
