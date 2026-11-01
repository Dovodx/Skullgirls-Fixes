#include "diagnostics/network_monitor.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <map>

#include "core/game_addresses.h"
#include "core/game_image.h"
#include "core/hooks.h"
#include "core/log.h"

namespace {

using namespace sgfixes;

// A peer counts as active for this long after our last packet to it.
constexpr uint64_t kPeerActiveMilliseconds = 3000;

// GGPO sends at least a keepalive every 200 ms, so silence past this is unusual.
constexpr uint64_t kTrafficGapMilliseconds = 500;

constexpr uint64_t kStatusPollMilliseconds = 1000;
constexpr uint64_t kStatusLogMilliseconds = 5000;

constexpr size_t kIdentitySize = 136;  // sizeof(SteamNetworkingIdentity)

// The first fields of Steam's SteamNetConnectionRealTimeStatus_t. Steam writes the whole
// struct, so the padding leaves room for the fields we don't use.
struct SteamConnectionStatus {
    int state;
    int pingMilliseconds;
    float qualityLocal;
    float qualityRemote;
    float outPacketsPerSecond;
    float outBytesPerSecond;
    float inPacketsPerSecond;
    float inBytesPerSecond;
    int sendRateBytesPerSecond;
    int pendingUnreliableBytes;
    int pendingReliableBytes;
    int sentUnackedReliableBytes;
    unsigned char unusedFields[256];
};

// Slot 5 of ISteamNetworkingMessages. The connection info argument is optional.
constexpr size_t kGetSessionConnectionInfoSlot = 5;
using GetSessionConnectionInfo = int(__thiscall*)(void* self, const void* identity, void* info,
                                                  SteamConnectionStatus* status);

struct Peer {
    unsigned char identity[kIdentitySize];
    bool isSpectator = false;
    uint64_t lastSendMilliseconds = 0;
    uint64_t lastReceiveMilliseconds = 0;
    uint64_t lastStatusLogMilliseconds = 0;
    uint32_t packetsSent = 0;
    uint32_t packetsReceived = 0;
    int lastSteamState = -100;
    bool receiveGapReported = false;
    bool sendGapReported = false;
};

// Both of these are only touched by the main thread, where the game runs all networking.
std::map<uint64_t, Peer> g_peers;
uint64_t g_lastStatusPollMilliseconds = 0;

// -1 means the game hasn't checked yet.
int g_loggedOn = -1;

using TransportSend = void(__thiscall*)(void* self, const void* data, int length, void* peer);
using TransportReceive = bool(__thiscall*)(void* self, void* buffer, unsigned capacity,
                                           unsigned* length, void* peer);
using IsLoggedOnToSteam = uint32_t(__cdecl*)();

TransportSend g_originalSend = nullptr;
TransportReceive g_originalReceive = nullptr;
IsLoggedOnToSteam g_originalIsLoggedOn = nullptr;

// Makes a short tag for a peer. Steam IDs identify real people, so logs only show enough
// to tell peers apart.
//   steamId: the peer's 64-bit Steam ID.
// Returns the low 16 bits of the ID.
unsigned PeerTag(uint64_t steamId) {
    return static_cast<unsigned>(steamId & 0xffff);
}

// Converts a connection state to text.
//   state: a value of Steam's ESteamNetworkingConnectionState.
// Returns the state's name, or "Other" for internal states.
const char* SteamStateName(int state) {
    switch (state) {
        case 0: return "None";
        case 1: return "Connecting";
        case 2: return "FindingRoute";
        case 3: return "Connected";
        case 4: return "ClosedByPeer";
        case 5: return "ProblemDetectedLocally";
        default: return "Other";
    }
}

// Finds a peer's record, creating it the first time, and updates its details.
//   gamePeer: the game's PeerAddress struct, with offsets listed in game_addresses.h.
// Returns the peer's record.
Peer& FindPeer(void* gamePeer) {
    auto* bytes = static_cast<unsigned char*>(gamePeer);
    const unsigned char* identity = bytes + game_addresses::kPeerIdentityOffset;

    uint64_t steamId;
    std::memcpy(&steamId, identity + game_addresses::kIdentitySteamIdOffset, sizeof(steamId));

    Peer& peer = g_peers[steamId];
    peer.isSpectator = bytes[game_addresses::kPeerIsSpectatorOffset] != 0;
    std::memcpy(peer.identity, identity, kIdentitySize);
    return peer;
}

// Runs in place of SteamTransport's send method. It records the packet, then sends it.
//   self:     the SteamTransport object.
//   data:     the packet to send.
//   length:   the packet's size in bytes.
//   gamePeer: the destination, as a PeerAddress struct.
void __thiscall OnTransportSend(void* self, const void* data, int length, void* gamePeer) {
    Peer& peer = FindPeer(gamePeer);
    peer.lastSendMilliseconds = log::SteadyMilliseconds();
    peer.packetsSent++;
    g_originalSend(self, data, length, gamePeer);
}

// Runs in place of SteamTransport's receive method. It receives a packet, then records it.
//   self:     the SteamTransport object.
//   buffer:   output. Receives the packet.
//   capacity: the buffer's size in bytes.
//   length:   output. Receives the packet's size.
//   gamePeer: output. Receives the sender, as a PeerAddress struct.
// Returns true if a packet was received, exactly as the game's method does.
bool __thiscall OnTransportReceive(void* self, void* buffer, unsigned capacity, unsigned* length,
                                   void* gamePeer) {
    bool received = g_originalReceive(self, buffer, capacity, length, gamePeer);
    if (received) {
        Peer& peer = FindPeer(gamePeer);
        peer.lastReceiveMilliseconds = log::SteadyMilliseconds();
        peer.packetsReceived++;
    }
    return received;
}

// Runs in place of the game's Steam login check. The game checks this every frame while
// online, and a single false result ends the match, so this logs every change of state.
// Returns the original result unchanged. See game_addresses.h for its format.
uint32_t __cdecl OnIsLoggedOnToSteam() {
    uint32_t result = g_originalIsLoggedOn();
    int loggedOn = (result & 0xff) != 0 ? 1 : 0;
    if (loggedOn != g_loggedOn) {
        log::Writef("steam", "Logged on to Steam: %s", loggedOn != 0 ? "yes" : "no");
        g_loggedOn = loggedOn;
    }
    return result;
}

// Gets the same Steam networking interface the game uses.
// Returns the ISteamNetworkingMessages pointer, or null if Steam isn't ready.
void* NetworkingMessagesInterface() {
    using ContextInit = void** (__cdecl*)(void* context);
    auto importSlot = reinterpret_cast<ContextInit*>(
        game_image::Rebase(game_addresses::kSteamContextInitImport));
    void* context = reinterpret_cast<void*>(
        game_image::Rebase(game_addresses::kNetworkingMessagesContext));

    void** interfaceSlot = (*importSlot)(context);
    return interfaceSlot == nullptr ? nullptr : *interfaceSlot;
}

// Asks Steam about a peer's connection, and logs the result if the state changed or a
// status line is due.
//   steamId: the peer's Steam ID.
//   peer:    the peer's record, which is updated.
//   now:     the current time from log::SteadyMilliseconds.
void PollSteamStatus(uint64_t steamId, Peer& peer, uint64_t now) {
    void* networking = NetworkingMessagesInterface();
    if (networking == nullptr) {
        return;
    }
    void** table = *static_cast<void***>(networking);
    auto getInfo = reinterpret_cast<GetSessionConnectionInfo>(table[kGetSessionConnectionInfoSlot]);

    SteamConnectionStatus status{};
    int state = getInfo(networking, peer.identity, nullptr, &status);

    bool stateChanged = state != peer.lastSteamState;
    if (stateChanged || now - peer.lastStatusLogMilliseconds >= kStatusLogMilliseconds) {
        log::Writef("net",
                    "Peer %04x%s: %s, ping %d ms, quality local %.2f remote %.2f, "
                    "packets sent %u received %u, Steam in %.0f/s out %.0f/s, queued %d bytes",
                    PeerTag(steamId), peer.isSpectator ? " (spectator)" : "", SteamStateName(state),
                    status.pingMilliseconds, status.qualityLocal, status.qualityRemote,
                    peer.packetsSent, peer.packetsReceived, status.inPacketsPerSecond,
                    status.outPacketsPerSecond,
                    status.pendingUnreliableBytes + status.pendingReliableBytes);
        peer.lastSteamState = state;
        peer.lastStatusLogMilliseconds = now;
    }
}

// Logs when traffic stops and when it returns.
//   steamId:    the peer's Steam ID.
//   direction:  "sent" or "received", used in the log line.
//   lastPacket: when the last packet went this way. 0 means none has yet.
//   now:        the current time from log::SteadyMilliseconds.
//   reported:   input and output. True while a gap has already been logged.
void CheckTrafficGap(uint64_t steamId, const char* direction, uint64_t lastPacket, uint64_t now,
                     bool& reported) {
    if (lastPacket == 0) {
        return;
    }
    bool inGap = now - lastPacket > kTrafficGapMilliseconds;
    if (inGap && !reported) {
        log::Writef("net", "Peer %04x: no packets %s for %u ms", PeerTag(steamId), direction,
                    static_cast<unsigned>(now - lastPacket));
    } else if (!inGap && reported) {
        log::Writef("net", "Peer %04x: packets %s again", PeerTag(steamId), direction);
    }
    reported = inGap;
}

}  // namespace

namespace sgfixes::diagnostics {

bool CreateNetworkMonitorHooks() {
    using namespace game_addresses;
    return hooks::Create("TransportSend", kTransportSend, kTransportSendBytes,
                         reinterpret_cast<void*>(OnTransportSend), &g_originalSend)
           && hooks::Create("TransportReceive", kTransportReceive, kTransportReceiveBytes,
                            reinterpret_cast<void*>(OnTransportReceive), &g_originalReceive)
           && hooks::Create("IsLoggedOnToSteam", kIsLoggedOnToSteam, kIsLoggedOnToSteamBytes,
                            reinterpret_cast<void*>(OnIsLoggedOnToSteam), &g_originalIsLoggedOn);
}

void UpdateNetworkMonitor(uint64_t nowMilliseconds) {
    bool pollStatus = nowMilliseconds - g_lastStatusPollMilliseconds >= kStatusPollMilliseconds;
    if (pollStatus) {
        g_lastStatusPollMilliseconds = nowMilliseconds;
    }

    for (auto& [steamId, peer] : g_peers) {
        bool active = nowMilliseconds - peer.lastSendMilliseconds < kPeerActiveMilliseconds;
        if (!active) {
            continue;
        }
        CheckTrafficGap(steamId, "received", peer.lastReceiveMilliseconds, nowMilliseconds,
                        peer.receiveGapReported);
        CheckTrafficGap(steamId, "sent", peer.lastSendMilliseconds, nowMilliseconds,
                        peer.sendGapReported);
        if (pollStatus) {
            PollSteamStatus(steamId, peer, nowMilliseconds);
        }
    }
}

}  // namespace sgfixes::diagnostics
