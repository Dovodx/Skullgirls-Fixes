// Copies the game's internal log messages into the mod's log.
//
// This adds millisecond timestamps, puts everything in one timeline, and captures
// messages that skullgirls.log never contains. The game's own logging is unaffected.
#pragma once

namespace sgfixes::diagnostics {

// Prepares the hooks on the game's three log functions.
// Returns true if they were all prepared.
bool CreateGameLogCaptureHooks();

}  // namespace sgfixes::diagnostics
