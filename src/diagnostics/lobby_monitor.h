// Records the state of every lobby member, including players without the mod.
//
// The game logs only its own player's status. Many lobby bugs depend on another member's
// status, such as a countdown that can't start while anyone is marked as in-game. So this
// reads each member's status from Steam, and logs every change. It also logs which
// spectators this player is expecting to connect.
#pragma once

#include <cstdint>

namespace sgfixes::diagnostics {

// Checks for lobby changes several times a second. Must be called on the main thread,
// once per tick.
//   nowMilliseconds: the current time from log::SteadyMilliseconds.
void UpdateLobbyMonitor(uint64_t nowMilliseconds);

}  // namespace sgfixes::diagnostics
