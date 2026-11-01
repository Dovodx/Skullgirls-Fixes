// Records the health of the connection to each GGPO peer: gaps in traffic, Steam's own
// view of the connection, and changes in Steam login state. These separate the possible
// causes of a disconnect.
#pragma once

#include <cstdint>

namespace sgfixes::diagnostics {

// Prepares the hooks on GGPO's send and receive functions and the Steam login check.
// Returns true if they were all prepared.
bool CreateNetworkMonitorHooks();

// Checks each active peer for traffic gaps, and polls Steam's connection status about
// once a second. Must be called on the main thread, once per tick.
//   nowMilliseconds: the current time from log::SteadyMilliseconds.
void UpdateNetworkMonitor(uint64_t nowMilliseconds);

}  // namespace sgfixes::diagnostics
