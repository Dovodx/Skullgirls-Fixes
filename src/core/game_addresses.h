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

// Offsets inside the game's PeerAddress struct.
constexpr size_t kPeerIsSpectatorOffset = 0;  // bool: uses the spectator channel
constexpr size_t kPeerIdentityOffset = 9;     // SteamNetworkingIdentity

// Offset of the 64-bit Steam ID inside a SteamNetworkingIdentity.
constexpr size_t kIdentitySteamIdOffset = 8;

}  // namespace sgfixes::game_addresses
