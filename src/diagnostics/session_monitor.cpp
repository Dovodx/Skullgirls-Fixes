#include "diagnostics/session_monitor.h"

#include <algorithm>
#include <cstring>

#include "core/game_addresses.h"
#include "core/game_image.h"
#include "core/hooks.h"
#include "core/log.h"
#include "core/steam_interfaces.h"

namespace {

using namespace sgfixes;
using namespace sgfixes::game_addresses;

using GgpoCloseSession = int(__cdecl*)(void* session);

GgpoCloseSession g_originalCloseSession = nullptr;

// Reads a value from inside one of the game's objects.
//   object: the object's address.
//   offset: the value's offset, from game_addresses.h.
// Returns the value.
template <typename Value>
Value ReadField(const void* object, size_t offset) {
    Value value;
    std::memcpy(&value, static_cast<const unsigned char*>(object) + offset, sizeof(value));
    return value;
}

// Converts the state of a GGPO connection to text.
//   state: the connection's state, as described in game_addresses.h.
// Returns the state's name, or "Other" for anything unexpected.
const char* ConnectionStateName(int state) {
    switch (state) {
        case 0: return "Syncing";
        case 1: return "Synchronized";
        case 2: return "Running";
        case 3: return "Disconnected";
        default: return "Other";
    }
}

// Logs a player's session. A player only sends spectators frames that have every player's
// input, so a confirmed frame behind the current one means the spectator hasn't been sent
// the newest frames.
//   session: the player's session.
//   caller:  where in the game's code the close was called from.
void LogPlayerSession(const void* session, uintptr_t caller) {
    int spectatorCount = ReadField<int>(session, kPlayerSessionSpectatorCountOffset);
    log::Writef("session",
                "Closing session as a player (returns to 0x%08x): at frame %d, "
                "confirmed to frame %d, %d spectators",
                static_cast<unsigned>(caller),
                ReadField<int>(session, kPlayerSessionCurrentFrameOffset),
                ReadField<int>(session, kPlayerSessionConfirmedFrameOffset), spectatorCount);

    int listedCount = std::clamp(spectatorCount, 0, kMaxSpectators);
    for (int index = 0; index < listedCount; index++) {
        const unsigned char* connection = static_cast<const unsigned char*>(session)
                                          + kPlayerSessionSpectatorsOffset
                                          + index * kConnectionSize;
        uint64_t steamId = ReadField<uint64_t>(
            connection, kConnectionPeerAddressOffset + kPeerIdentityOffset + kIdentitySteamIdOffset);
        log::Writef("session", "Spectator %04x: %s, next frame to send %d, acknowledged to frame %d",
                    steam_interfaces::ShortTag(steamId),
                    ConnectionStateName(ReadField<int>(connection, kConnectionStateOffset)),
                    ReadField<int>(session, kPlayerSessionNextSpectatorFrameOffset
                                                + index * sizeof(int)),
                    ReadField<int>(connection, kConnectionLastAckedFrameOffset));
    }
}

// Logs a spectator's session. If the newest frame received is behind the next frame
// needed, the spectator ran out of frames to show.
//   session: the spectator's session.
//   caller:  where in the game's code the close was called from.
void LogSpectatorSession(const void* session, uintptr_t caller) {
    int newestFrame = -1;
    for (size_t index = 0; index < kSpectatorSessionInputCount; index++) {
        size_t offset = kSpectatorSessionInputsOffset + index * kSpectatorSessionInputSize;
        newestFrame = std::max(newestFrame, ReadField<int>(session, offset));
    }
    log::Writef("session",
                "Closing session as a spectator (returns to 0x%08x): needs frame %d, "
                "received up to frame %d",
                static_cast<unsigned>(caller),
                ReadField<int>(session, kSpectatorSessionNextFrameOffset), newestFrame);
}

// Runs in place of ggpo_close_session. It logs the session's state while the session
// still exists, then closes it exactly as the game asked.
//   session: the session to close. The game sometimes passes null, which GGPO ignores.
// Returns GGPO's result unchanged.
int __cdecl OnGgpoCloseSession(void* session) {
    if (session != nullptr) {
        // The return address shows which of the game's four callers closed the session.
        uintptr_t caller = game_image::ToPreferred(
            reinterpret_cast<uintptr_t>(__builtin_return_address(0)));
        uintptr_t table = ReadField<uintptr_t>(session, 0);
        if (table == game_image::Rebase(kPlayerSessionVtable)) {
            LogPlayerSession(session, caller);
        } else if (table == game_image::Rebase(kSpectatorSessionVtable)) {
            LogSpectatorSession(session, caller);
        } else {
            log::Writef("session", "Closing another kind of session (returns to 0x%08x)",
                        static_cast<unsigned>(caller));
        }
    }
    return g_originalCloseSession(session);
}

}  // namespace

namespace sgfixes::diagnostics {

bool CreateSessionMonitorHooks() {
    return hooks::Create("GgpoCloseSession", kGgpoCloseSession, kGgpoCloseSessionBytes,
                         reinterpret_cast<void*>(OnGgpoCloseSession), &g_originalCloseSession);
}

}  // namespace sgfixes::diagnostics
