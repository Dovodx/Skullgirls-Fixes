// Facts about the running game executable.
#pragma once

#include <cstdint>

namespace sgfixes::game_image {

// Checks whether the running exe is the one build the mod supports, by comparing the
// SHA-256 hash of the file on disk.
// Returns true on a match. Also returns false if the file can't be read or hashed.
bool IsSupportedBuild();

// Finds where Windows loaded the exe. This can differ from its preferred address.
// Returns the exe's base address in this process.
uintptr_t LoadedBase();

// Converts an address from game_addresses.h to its real address in this process.
//   preferredAddress: the address as listed in game_addresses.h and shown by Ghidra.
// Returns the address to use at runtime.
uintptr_t Rebase(uintptr_t preferredAddress);

}  // namespace sgfixes::game_image
