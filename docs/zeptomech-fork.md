# Keeping the Zeptomech fork easy to update

Branch `zeptomech-v8` sits directly on upstream schollz/_core (`5613dde0`, v8.1.0 + 8 commits), so upstream changes come in with an ordinary rebase or merge. The fork is mostly new files. Upstream files only get short `#ifdef INCLUDE_ZEPTOMECH` additions, and just 2 upstream lines are edited.

Zeptomech differences go behind `INCLUDE_ZEPTOMECH` and leave upstream (RP2040) behaviour as the default. The one exception is `lib/sdio/CMakeLists.txt`, which runs before the compile definitions are read and so uses `PICO_RP2350`.

## 1. Track the three files copied from upstream

The biggest risk: three files started as copies of upstream files, so upstream fixes to the originals do not reach them.

| Zeptomech copy | Upstream original |
|---|---|
| `lib/zeptomech.h` | `lib/zeptocore.h` |
| `lib/midi_comm_zeptomech.h` | `lib/midi_comm.h` |
| `lib/usb_descriptors_zeptomech.c` | `lib/usb_descriptors.c` |

`scripts/sync_zeptomech_copies.sh` (or `make sync-zeptomech-copies`) handles this. After a rebase or merge onto new upstream, it takes what changed in each original since the base recorded in `scripts/zeptomech_upstream_base`, and 3-way applies it to the copy with `git apply -3`. It then moves the recorded base forward.

- `UPSTREAM=<commit>` picks the new base; the default is `upstream/main`. The new base must already be in the branch.
- It lists which originals changed. Conflicts are left as merge markers in the copy and the script exits with an error.
- The copies need to be committed first, so that what the script changed is easy to see with `git diff`.
- Even a clean apply deserves a look: an upstream change can depend on code the copy has replaced.

## 2. Keep the edits to upstream files small

Each upstream file gets short hooks; the code lives in Zeptomech files:

| Upstream file | Hook | Zeptomech file |
|---|---|---|
| `main.c` | include, `zeptomech_setup()` call, plus clock/DCDC/WS2812 `#ifdef`s | `lib/zeptomech_main.h` (LED render, hardware setup) |
| `lib/globals.h` | one include | `lib/zeptomech_globals.h` |
| `lib/midi_out.h` | redirects its usb midi writes, like `midi_comm.h`'s `printf` | `lib/midi_out_zeptomech.h` (TRS copy of notes and ccs) |
| `lib/includes.h` | Zeptomech includes, `midi_comm` swap | |
| `lib/leds2.h`, `lib/button_handler.h`, `lib/midicallback.h` | small `#ifdef` blocks | |
| `CMakeLists.txt`, `tusb_config.h`, `lib/sdio/CMakeLists.txt`, `Makefile` | build switches | `lib/cmake/zeptomech_compile_definitions.cmake`, `lib/boards/` |

New Zeptomech code should follow the same pattern: put it in a Zeptomech file and add a one-line hook upstream, rather than inline blocks.

## 3. Send generic hooks upstream

Each one schollz accepts is a change the fork no longer carries. Candidates:

- the CMake switch that picks a per-target USB descriptor file;
- the CDC switch in `tusb_config.h`;
- the RP2350 `hardware_rtc` stub in `lib/sdio/CMakeLists.txt`;
- the `#ifdef SIMULATOR` hooks.

Upstream support for a board-specific header, included when its define is set, would remove most of the `INCLUDE_ZEPTOMECH` lines in `lib/includes.h`.

## 4. Update routine

- **Rebase** `zeptomech-v8` onto each new upstream release. This keeps the fork as a few clear commits on top of upstream; it needs a force-push (`--force-with-lease`). For a solo fork this is the better choice.
- **Or merge** upstream into the branch, which never rewrites history.

Either way:

- run `git config rerere.enabled true` once, so git remembers each conflict resolution and repeats it next time;
- run the sync script afterwards; `scripts/zeptomech_upstream_base` records the base the copies are in step with, so no tags are needed.

Typical update:

```sh
git fetch upstream
git rebase upstream/main zeptomech-v8
make sync-zeptomech-copies      # section 1; resolve any conflicts
# run the checks (section 5), then commit the synced copies and base file
git push --force-with-lease origin zeptomech-v8
```

## 5. One check after each update

For example `make check-fork`:

1. Build `zeptocore` and `ectocore` and compare their flash size with the same builds from upstream alone. Any difference means Zeptomech code has leaked into an upstream target.
2. Build `zeptomech`.
3. Run the simulator's `--quit` scripts (`../zeptomech-sim`) as smoke tests.

## 6. Keep commits split by topic

Keep the simulator hooks in their own commit, and the hook refactor from section 2 too. When a rebase conflicts, it is then obvious which feature the conflict belongs to.

## Remaining

1. Add `make check-fork` (section 5).
2. Offer the generic hooks upstream (section 3).
