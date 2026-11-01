// Installs function hooks. A hook redirects a game function to one of ours.
//
// Hooks are created first and then enabled together, so the game is never left with
// only some of them active. Before creating a hook, this checks that the target still
// has its expected first bytes.
#pragma once

#include <cstddef>
#include <cstdint>

namespace sgfixes::hooks {

// Prepares the hooking library. Must be called once before creating hooks.
void Begin();

// Prepares a hook on a game function. The hook has no effect until EnableAll runs.
//   name:              short name to identify this hook in the log.
//   preferredAddress:  the function's address from game_addresses.h.
//   expectedBytes:     the bytes the function should start with.
//   expectedByteCount: how many bytes to compare.
//   detour:            our function, which will run in place of the game's.
//   original:          output. Receives a pointer that runs the game's unmodified code.
// Returns true if the hook was prepared. On failure, the reason is logged.
bool Create(const char* name, uintptr_t preferredAddress, const uint8_t* expectedBytes,
            size_t expectedByteCount, void* detour, void** original);

// Same as Create above, with the byte count taken from the array's size. Original can be
// any function pointer type, so callers don't need to cast it.
//   name, preferredAddress, detour: as above.
//   expectedBytes: array of the bytes the function should start with.
//   original:      output. Receives a typed pointer to the game's unmodified code.
// Returns true if the hook was prepared.
template <size_t ByteCount, typename Original>
bool Create(const char* name, uintptr_t preferredAddress, const uint8_t (&expectedBytes)[ByteCount],
            void* detour, Original* original) {
    return Create(name, preferredAddress, expectedBytes, ByteCount, detour,
                  reinterpret_cast<void**>(original));
}

// Turns on every prepared hook at once.
// Returns true if they were all enabled.
bool EnableAll();

// Turns off every hook and restores the game's original code.
void RemoveAll();

}  // namespace sgfixes::hooks
