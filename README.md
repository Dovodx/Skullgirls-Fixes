# Skullgirls Fixes

A work-in-progress mod for Skullgirls 2nd Encore (Windows build, also run through
Proton on Linux). It aims to fix online problems: match disconnects, lobby
failures, and spectators who fail to load into a match.

**Status: early development. There is nothing to install yet.**

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

The mod will load through a proxy `Tolk.dll`, which lives in the game folder and
needs no Wine DLL overrides. Other community mods commonly use `d3d9.dll` or
`dbghelp.dll`, so this choice is intended not to collide with them.

## Getting the source

```bash
git clone --recurse-submodules <repository-url>
```

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
