#!/bin/sh
# Removes Skullgirls Fixes by putting the game's own Tolk.dll back. Logs stay in sgfixes-logs.
set -e
cd "$(dirname "$0")/.."
[ -f Tolk_orig.dll ] || { echo "Nothing to restore: Tolk_orig.dll isn't in the game folder."; exit 0; }
mv -f Tolk_orig.dll Tolk.dll
echo "Removed Skullgirls Fixes. You can now delete the sgfixes folder."
