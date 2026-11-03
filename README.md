# Skullgirls Fixes

A work-in-progress mod for Skullgirls 2nd Encore (Windows build, also run through
Proton on Linux). It aims to fix online problems: match disconnects, lobby
failures, and spectators who fail to load into a match.

**Status: early development.** The current build only records diagnostics. It doesn't
fix anything yet, and it doesn't change how the game plays.

## Goals

- Fix bugs without changing the game's simulation or replay format. Replays from
  modded clients must stay playable on unmodded clients.
- Keep playing with unmodded clients wherever possible, with no toggle needed.
- Coexist with other Skullgirls mods.
- Keep the source easy to read, so anyone can check exactly what the mod does.

## Supported game version

The mod patches specific addresses, so it only supports one exact build. It will
check the executable and refuse to patch any other version.

| Field | Value |
|---|---|
| Version | `RETAIL-3.7.14` (`R34085`) |
| `SkullGirls.exe` SHA-256 | `fb757433d2d11602ded18040078e70319afdae98c35fc22d789b38d012ad400b` |

## Technology

- C++20, built with the mingw-w64 `i686` toolchain as a 32-bit Windows DLL.
- CMake and Ninja.
- [MinHook](https://github.com/TsudaKageyu/minhook) for function hooks, included as
  a git submodule in `external/minhook`.
- `scripts/` holds the Ghidra scripts used for static analysis of the game.
- `install/` holds the install and uninstall scripts for Windows and Linux.

The mod will load through a proxy `Tolk.dll`, which lives in the game folder and
needs no Wine DLL overrides. Other community mods commonly use `d3d9.dll` or
`dbghelp.dll`, so this choice is intended not to collide with them.

## Getting the source

```bash
git clone --recurse-submodules <repository-url>
```

## Building

You need mingw-w64 for `i686`, CMake and Ninja.

```bash
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw-i686.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

This produces `build/Tolk.dll`. Running the tests on Linux needs Wine.

To build the download, `build/skullgirls-fixes.zip`:

```bash
cmake --build build --target release_zip
```

## Installing

1. Extract the download into the game folder. This adds an `sgfixes` folder and
   changes nothing else.
2. Run the install script in that folder:
   - **Windows:** double-click `sgfixes\install.bat`.
   - **Linux (Proton):** in the game folder, run `sh sgfixes/install.sh`.

The script keeps the game's `Tolk.dll` as `Tolk_orig.dll` and puts the mod's `Tolk.dll`
in its place. To update, extract a newer download over the old one and run the script
again. Run it again too after a Steam update or file check, since those put the
original `Tolk.dll` back.

To uninstall, run `sgfixes\uninstall.bat` or `sh sgfixes/uninstall.sh`, then delete the
`sgfixes` folder. The mod's logs in `sgfixes-logs` are left in place. Steam's "Verify
integrity of game files" also restores the original `Tolk.dll`, which turns the mod off.

To install by hand instead:

1. In the game folder, rename the existing `Tolk.dll` to `Tolk_orig.dll`.
2. Copy `sgfixes/Tolk.dll` into the game folder.

To uninstall by hand, delete the mod's `Tolk.dll` and rename `Tolk_orig.dll` back.
Leaving `Tolk_orig.dll` behind is harmless.

## Diagnostic logs

Each launch writes a new file to `sgfixes-logs` in the game folder. It records:

- The game's log messages with millisecond timestamps, including messages that
  `skullgirls.log` never contains
- Pauses of the game's main thread
- Gaps in match traffic, and Steam's view of each connection: state, ping, and quality
- Changes in Steam login state
- Every lobby member's ready and in-game status, including players without the mod
- Which spectators each player is expecting, and which ones connect
- How far each match session got when it closed: the current frame, the last frame
  with both players' inputs, and the frames sent to and received by each spectator

Full Steam IDs and the user folder in file paths are removed from every line, and peers
are shown as a short 4-character tag. Logs still contain Steam display names, including
those of other players, so check that you're comfortable with that before sharing one.

## Code standards

- Write for human readers: clear names, small functions, and straightforward code
  over clever code.
- Document every hook and patch: what the game does at that address, why the mod
  changes it, what evidence supports the change, and how it stays compatible with
  unmodded clients and replays.
- Verify the original bytes before every patch, and fail safely on a mismatch.
- Don't commit game files, decompiled game code, logs, or other players' details.

## License

MIT, see [LICENSE](LICENSE). MinHook is distributed under its own license in
`external/minhook`. Skullgirls is the property of its respective owners, and this
project is not affiliated with them.
