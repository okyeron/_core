#!/usr/bin/env bash
# Carries upstream changes into the Zeptomech copies of upstream files.
#
# lib/zeptomech.h, lib/midi_comm_zeptomech.h and lib/usb_descriptors_zeptomech.c
# started as copies of upstream files, so git does not bring upstream fixes to
# the originals into them. Run this after rebasing (or merging) onto a new
# upstream: it takes what changed in each original between the recorded base
# and the new one and 3-way applies it to the copy. Conflicts are left as merge
# markers in the copy.
#
#   scripts/sync_zeptomech_copies.sh [new-base]    (default: upstream/main)
#
# The base the copies are in sync with is kept in
# scripts/zeptomech_upstream_base and moved to the new base after a run.
# See docs/zeptomech-fork.md.

set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

BASE_FILE=scripts/zeptomech_upstream_base

# copy:original
PAIRS=(
  "lib/zeptomech.h:lib/zeptocore.h"
  "lib/midi_comm_zeptomech.h:lib/midi_comm.h"
  "lib/usb_descriptors_zeptomech.c:lib/usb_descriptors.c"
)

old=$(git rev-parse --verify "$(tr -d '[:space:]' <"$BASE_FILE")^{commit}")
new=$(git rev-parse --verify "${1:-upstream/main}^{commit}")

if [ "$old" = "$new" ]; then
  echo "copies already in sync with ${new:0:8}"
  exit 0
fi
if ! git merge-base --is-ancestor "$new" HEAD; then
  echo "${new:0:8} is not in this branch yet: rebase or merge onto it first" >&2
  exit 1
fi

copies=()
for pair in "${PAIRS[@]}"; do copies+=("${pair%%:*}"); done
if ! git diff --quiet HEAD -- "${copies[@]}"; then
  echo "commit or stash the changes to ${copies[*]} first" >&2
  exit 1
fi

echo "syncing copies from ${old:0:8} to ${new:0:8}"
conflicts=0
for pair in "${PAIRS[@]}"; do
  copy=${pair%%:*}
  orig=${pair#*:}
  if ! git cat-file -e "$new:$orig" 2>/dev/null; then
    echo "  $orig: gone upstream, check by hand what replaced it ($copy)"
    conflicts=1
    continue
  fi
  if git diff --quiet "$old" "$new" -- "$orig"; then
    echo "  $orig: unchanged"
    continue
  fi
  echo "  $orig: $(git diff --shortstat "$old" "$new" -- "$orig" | sed 's/^ *1 file changed, //')"
  # same patch, retargeted at the copy; -3 merges against the old original
  if git diff "$old" "$new" -- "$orig" |
    sed -e "s#^diff --git a/$orig b/$orig\$#diff --git a/$copy b/$copy#" \
      -e "s#^--- a/$orig\$#--- a/$copy#" \
      -e "s#^+++ b/$orig\$#+++ b/$copy#" |
    git apply -3; then
    echo "    applied to $copy"
  else
    echo "    CONFLICTS in $copy"
    conflicts=1
  fi
done

echo "$new" >"$BASE_FILE"
echo "base moved to ${new:0:8} ($BASE_FILE)"
if [ "$conflicts" -ne 0 ]; then
  echo "resolve the conflicts above, then build zeptomech and commit" >&2
  exit 1
fi
echo "review with 'git diff', build zeptomech, then commit"
