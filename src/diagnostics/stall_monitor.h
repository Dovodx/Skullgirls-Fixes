// Detects when the game's main thread stops running.
//
// All of the game's networking runs on the main thread, so while it's frozen, nothing
// is sent or received. A freeze of about 5 seconds makes the opponent's game time out.
#pragma once

namespace sgfixes::diagnostics {

// Prepares the hook on the game's main loop.
// Returns true if it was prepared.
bool CreateStallMonitorHooks();

// Starts a thread that reports a freeze while it's still happening, in case the game
// never recovers.
void StartStallWatchdog();

}  // namespace sgfixes::diagnostics
