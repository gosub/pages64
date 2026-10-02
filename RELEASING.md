# Releasing pages64

The release procedure. CI does most of the work: pushing a `v*` tag builds all
four platforms, creates the GitHub release and attaches the artifacts. What is
left here is getting the repository into a consistent state *before* the tag,
and telling the VCV Library about it after. (Adapted from forsitan modulare's
procedure, along with the scripts in `tools/release/`.)

## Version numbers

| bump | when |
|------|------|
| **minor** `2.x.0` | a new module is added |
| **patch** `2.x.y` | fixes and enhancements to existing modules |

`plugin.json` `"version"` and the git tag must match exactly, tag prefixed
with `v`: version `2.22.0` is tag `v2.22.0`. CI's publish job compares them
and fails the release if they differ.

## Before the tag

1. **Green build.** Every push builds win-x64, lin-x64, mac-x64 and
   mac-arm64 and runs the tests. Check the run for the commit you intend to
   tag.

2. **Tests pass.**

   ```
   make -C test -j check
   ```

   Persistence round trips for every module, legacy-format loading, and the
   single-purpose checks (see `test/README.md`). Nonzero on any failure.

3. **No two modules define the same symbol.**

   ```
   python3 tools/release/check_symbols.py
   ```

   Every module is its own translation unit linked into one plugin, so two
   file-scope types of the same name are an ODR violation: MinGW fails the
   Windows job *after* the tag is pushed, ELF silently runs one module's code
   in the other. Needs a built tree (`make`). Put anything file-local in an
   anonymous namespace.

4. **Panels are legal**, if any layout changed:

   ```
   python3 tools/panel_audit/panel_audit.py [Slug ...]
   ```

   Builds every ModuleWidget from the plugin's own objects and checks the
   real geometry: clearances between controls, labels, lights and screws,
   the panel edge, the title position and the active-page light. Needs the
   Rack install for its component SVGs (`RACK_SYSTEM_DIR`, default
   `/home/gg/dl/audio/Rack-2.6.6`).

5. **Documentation matches the code.** Every module needs `docs/<Slug>.md`, a
   README list entry and a `plugin.json` entry with its `manualUrl`. When a
   module was added this release, confirm all of it landed: `src/<Name>.cpp`,
   the model declared in `src/plugin.hpp` and registered in `src/plugin.cpp`,
   the panel `res/<Name>.svg`, the `plugin.json` entry, the doc page, the
   README entry.

6. **`plugin.json` tags are ones Rack knows.** The Library rejects a manifest
   with an unknown tag, after the tag is pushed.

   ```
   python3 tools/release/check_tags.py           # --fetch refreshes Rack's list
   ```

   Descriptions are one-line summaries: Rack shows them unwrapped as the
   module-browser tooltip, so keep them under ~100 characters.

7. **Regenerate the example patches** if a module they use changed:

   ```
   python3 tools/gen_patches.py
   ```

8. **Write the CHANGELOG entry.** Rename `## Unreleased` at the top to
   `## <version> — <YYYY-MM-DD>`. `git log` since the previous tag is the raw
   material.

9. **Bump `"version"` in `plugin.json`,** then make everything agree:

   ```
   python3 tools/release/sync_version.py
   ```

   This repoints every documentation URL (plugin `changelogUrl`, one
   `manualUrl` per module) at `v<version>`, so someone running an older build
   opens the manual their build matches. Never hand-edit them.

10. **Verify, then commit.**

   ```
   python3 tools/release/sync_version.py --check
   ```

   Fails if `plugin.json`, the newest CHANGELOG heading and the URLs disagree,
   or if an `## Unreleased` heading is left. It notes that the tag doesn't
   exist yet; that's expected here. Commit the result.

## Tag and publish

```
git tag -a v<version> -m "Version <version>: <summary>"
git push && git push --tags
```

The tag must be pushed: until it is, the documentation URLs 404, since they
name it. CI then verifies version against tag, builds the four platforms,
creates the GitHub release and uploads every `.vcvplugin`. Check the release
page.

## Tell the VCV Library

The Library builds from source on its own infrastructure; a tag alone doesn't
reach users.

- **First submission:** open an issue at
  [VCVRack/library](https://github.com/VCVRack/library/issues) with the plugin
  slug (`pages64`), the source URL and the license, following the template
  there. Record the issue link here once it exists.
- **Updates:** a comment on that issue:

  ```
  update:

  https://github.com/gosub/pages64
  version: <version>
  commit: <the tagged commit sha>
  ```

  Get the sha with `git rev-list -n 1 v<version>`.

## Local install, for checking a build by hand

```
HOME=/home/gg/dl/temp/rackhome/ make install
```

After a version bump, delete stale `pages64-*.vcvplugin` archives and the
extracted `pages64/` folder from that Rack home's `plugins-lin-x64/` first:
Rack extracts every archive at startup and an old one can overwrite the new.
