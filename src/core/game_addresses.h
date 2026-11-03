// Every location in SkullGirls.exe that the mod touches.
//
// These are only valid for the build in game_image.cpp. Addresses are written as the
// exe's preferred ones, which match what Ghidra shows. The exe can load elsewhere, so
// always convert them with game_image::Rebase().
//
// Each hook target also lists its first bytes, which are compared before patching. Leave
// out any byte that is part of an absolute address, because those change if the exe loads
// somewhere else.
#pragma once

#include <cstddef>
#include <cstdint>

namespace sgfixes::game_addresses {

// The game's normal logger, which writes skullgirls.log when -log is used.
// void __cdecl GameLog(Logger* logger, const char* format, ...)
constexpr uintptr_t kGameLog = 0x00531870;
constexpr uint8_t kGameLogBytes[] = {0x55, 0x8B, 0xEC, 0x8B, 0x4D, 0x08};

// A debug print that normally goes nowhere. It carries messages skullgirls.log doesn't
// have, such as "Attempting to shut down due to disconnection from online service".
// void __cdecl DebugPrint(const char* format, ...)
constexpr uintptr_t kDebugPrint = 0x004B9000;
constexpr uint8_t kDebugPrintBytes[] = {0x55, 0x8B, 0xEC, 0x6A, 0xFF};

// GGPO's internal logger, which is only active with -ggpolog.
// void __cdecl GgpoLog(const char* format, ...)
constexpr uintptr_t kGgpoLog = 0x0069E6E0;
constexpr uint8_t kGgpoLogBytes[] = {0x55, 0x8B, 0xEC, 0x8D, 0x45, 0x0C};

// Wrapper around SteamUser()->BLoggedOn(). It returns a bool, which only sets the lowest
// byte of the return register (AL, the bottom of EAX). The upper 24 bits hold leftover
// values, so only test the lowest byte of the result.
// uint32_t __cdecl IsLoggedOnToSteam()
constexpr uintptr_t kIsLoggedOnToSteam = 0x00463500;
constexpr uint8_t kIsLoggedOnToSteamBytes[] = {0x68};  // PUSH of an absolute address

// One tick of the game's main loop, called from the loop at 0x005141C7.
constexpr uintptr_t kMainLoopTick = 0x00518740;
constexpr uint8_t kMainLoopTickBytes[] = {0x53, 0x8B, 0xDC, 0x83, 0xEC, 0x08};

// SteamTransport methods, which carry all GGPO traffic. Both use the thiscall convention,
// MSVC's standard for C++ methods: the object pointer goes in the ECX register, the other
// arguments go on the stack, and the called function removes them from the stack.
// void SendTo(const void* data, int length, PeerAddress* peer)
constexpr uintptr_t kTransportSend = 0x0069BD70;
constexpr uint8_t kTransportSendBytes[] = {0x55, 0x8B, 0xEC, 0x56, 0x57};

// bool ReceiveFrom(void* buffer, unsigned capacity, unsigned* length, PeerAddress* peer)
constexpr uintptr_t kTransportReceive = 0x0069BCA0;
constexpr uint8_t kTransportReceiveBytes[] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x10};

// Import table slot holding the address of steam_api.dll's SteamInternal_ContextInit.
constexpr uintptr_t kSteamContextInitImport = 0x007333AC;

// The game's cached context for SteamNetworkingMessages002. Passing it to
// SteamInternal_ContextInit returns a pointer to the interface pointer.
constexpr uintptr_t kNetworkingMessagesContext = 0x00843750;

// The game's cached Steam contexts. Passing one to SteamInternal_ContextInit returns a
// pointer to the interface pointer, as above.
constexpr uintptr_t kMatchmakingContext = 0x00843744;  // SteamMatchMaking009
constexpr uintptr_t kFriendsContext = 0x0083577C;      // ISteamFriends

// Virtual function table slots, confirmed by the game's calls in the functions at
// 0x0053E890 and 0x00552D70.
constexpr size_t kGetLobbyMemberDataSlot = 24;
constexpr size_t kGetFriendPersonaNameSlot = 7;

// bool: whether the player is in a lobby. The game checks this before using kRoom.
constexpr uintptr_t kInRoomFlag = 0x00C52187;

// The game's lobby object.
constexpr uintptr_t kRoom = 0x00C530C0;
constexpr size_t kRoomLobbyIdOffset = 0;  // uint64

// Two std::vector<uint64_t> of Steam IDs, as begin and end pointers.
constexpr size_t kRoomMembersBeginOffset = 0x1C;
constexpr size_t kRoomMembersEndOffset = 0x20;
constexpr size_t kRoomExpectedSpectatorsBeginOffset = 0x50;
constexpr size_t kRoomExpectedSpectatorsEndOffset = 0x54;

// int: the game's GGPO session state. 0 means no session is running. Other values cover
// the session's phases, and the game resets this to 0 whenever it closes a session.
constexpr uintptr_t kGgpoSessionState = 0x00C52C94;

// Offsets inside the game's PeerAddress struct. The spectator flag is only reliable in
// addresses the game sends to. Playtest logs showed it missing from received ones.
constexpr size_t kPeerIsSpectatorOffset = 0;  // bool: uses the spectator channel
constexpr size_t kPeerIdentityOffset = 9;     // SteamNetworkingIdentity

// Offset of the 64-bit Steam ID inside a SteamNetworkingIdentity.
constexpr size_t kIdentitySteamIdOffset = 8;

// GGPO's exported ggpo_close_session. The game calls it from four places to end a match
// session, both as a player and as a spectator.
// int __cdecl ggpo_close_session(GGPOSession* session)
constexpr uintptr_t kGgpoCloseSession = 0x0069B970;
constexpr uint8_t kGgpoCloseSessionBytes[] = {0x55, 0x8B, 0xEC, 0x8B, 0x4D, 0x08};

// A GGPO session's first 4 bytes point to its class's virtual function table, which tells
// the two kinds apart. Their constructors at 0x0069BDD0 and 0x0069DD20 store these.
constexpr uintptr_t kPlayerSessionVtable = 0x00751054;     // GGPO's Peer2PeerBackend
constexpr uintptr_t kSpectatorSessionVtable = 0x007516D8;  // GGPO's SpectatorBackend

// Offsets inside a player's session, read by its methods at 0x0069CF00 and 0x0069CF20 and
// by its spectator loop at 0x0069CD2B.
constexpr size_t kPlayerSessionConfirmedFrameOffset = 0x4F4;  // int: last frame with every player's input
constexpr size_t kPlayerSessionCurrentFrameOffset = 0x4F8;    // int: the frame being simulated
constexpr size_t kPlayerSessionSpectatorsOffset = 0x5734;     // one connection per spectator
constexpr size_t kPlayerSessionSpectatorCountOffset = 0x1565B4;      // int
constexpr size_t kPlayerSessionNextSpectatorFrameOffset = 0x1565C8;  // int per spectator
constexpr int kMaxSpectators = 32;

// Offsets inside one of GGPO's connections to a peer (its UdpProtocol class). The state
// uses GGPO's order: 0 syncing, 1 synchronized, 2 running, 3 disconnected.
constexpr size_t kConnectionSize = 0xA874;
constexpr size_t kConnectionPeerAddressOffset = 8;          // PeerAddress, copied at 0x006A0220
constexpr size_t kConnectionStateOffset = 0x4EC;            // int
constexpr size_t kConnectionLastAckedFrameOffset = 0xA868;  // int, logged by GGPO as last_acked_input

// Offsets inside a spectator's session, read by its input function at 0x0069E200. Received
// inputs are kept in a ring, and each entry starts with its frame number, or -1 if empty.
constexpr size_t kSpectatorSessionNextFrameOffset = 0xAAD0;  // int: next frame for the game
constexpr size_t kSpectatorSessionInputsOffset = 0xAAD4;
constexpr size_t kSpectatorSessionInputSize = 0x38;
constexpr size_t kSpectatorSessionInputCount = 0x800;

}  // namespace sgfixes::game_addresses
