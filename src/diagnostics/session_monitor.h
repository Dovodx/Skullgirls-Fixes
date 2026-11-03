// Records how far each GGPO match session got at the moment the game closes it.
//
// Many spectator sessions end in GGPO's 5 second timeout instead of closing cleanly, and
// how often depends on which player the spectator watches through. One possible cause is
// that the player's game closes its session before it has sent the spectator the match's
// final frames. These lines show how far each side got, so the player's and spectator's
// logs can be compared.
#pragma once

namespace sgfixes::diagnostics {

// Prepares the hook on GGPO's function for closing a session.
// Returns true if it was prepared.
bool CreateSessionMonitorHooks();

}  // namespace sgfixes::diagnostics
