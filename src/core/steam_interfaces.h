// Read-only access to the Steam interfaces the game already uses.
//
// The mod doesn't use Valve's SDK. It gets each interface from the game's own cached
// context, and calls functions by their position in the interface's virtual function
// table. Those positions are confirmed by the game's own calls, listed in
// game_addresses.h.
#pragma once

#include <cstddef>
#include <cstdint>

namespace sgfixes::steam_interfaces {

// Gets a Steam interface through one of the game's cached contexts.
//   preferredContextAddress: a context address from game_addresses.h.
// Returns the interface pointer, or null if Steam isn't ready.
void* FromContext(uintptr_t preferredContextAddress);

// Looks up a function in an interface's virtual function table.
//   steamInterface: the interface pointer.
//   slot:           the function's position in the table.
// Returns the function's address.
void* VirtualFunction(void* steamInterface, size_t slot);

// Makes a short tag for a player. Steam IDs identify real people, so logs only show
// enough to tell players apart.
//   steamId: the player's 64-bit Steam ID.
// Returns the low 16 bits of the ID.
unsigned ShortTag(uint64_t steamId);

}  // namespace sgfixes::steam_interfaces
