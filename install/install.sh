#!/bin/sh
# Installs Skullgirls Fixes from the sgfixes folder, keeping the game's Tolk.dll as Tolk_orig.dll.
set -e
cd "$(dirname "$0")/.."
[ -f SkullGirls.exe ] || { echo "The sgfixes folder must be in the game folder, next to SkullGirls.exe."; exit 1; }
[ -f Tolk_orig.dll ] || mv Tolk.dll Tolk_orig.dll
# Make a new file instead of writing into the old one, which a running game may have open.
cp --remove-destination sgfixes/Tolk.dll Tolk.dll
echo "Installed Skullgirls Fixes."
