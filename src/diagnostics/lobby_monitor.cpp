#include "diagnostics/lobby_monitor.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "core/game_addresses.h"
#include "core/game_image.h"
#include "core/log.h"
#include "core/steam_interfaces.h"

namespace {

using namespace sgfixes;

constexpr uint64_t kPollMilliseconds = 100;

// A sanity limit in case the game's list is being changed or was misread.
constexpr size_t kMaxListedPlayers = 64;

// Shown for a value Steam hasn't delivered yet, which can matter in timing bugs.
constexpr const char* kUnknownValue = "?";

struct MemberStatus {
    std::string ready;
    std::string inGame;
    std::string opponent;
};

// All of these are only touched by the main thread.
std::map<uint64_t, MemberStatus> g_members;
std::vector<uint64_t> g_expectedSpectators;
uint64_t g_lastPollMilliseconds = 0;

using GetLobbyMemberData = const char*(__thiscall*)(void* self, uint64_t lobby, uint64_t member,
                                                    const char* key);
using GetFriendPersonaName = const char*(__thiscall*)(void* self, uint64_t player);

// Reads a list of Steam IDs held in a std::vector inside the game's room object.
//   room:        address of the room object.
//   beginOffset: offset of the vector's start pointer.
//   endOffset:   offset of the vector's end pointer.
// Returns the IDs, or an empty list if the vector doesn't look valid.
std::vector<uint64_t> ReadSteamIdList(uintptr_t room, size_t beginOffset, size_t endOffset) {
    auto begin = *reinterpret_cast<const uint64_t* const*>(room + beginOffset);
    auto end = *reinterpret_cast<const uint64_t* const*>(room + endOffset);
    if (begin == nullptr || end < begin || static_cast<size_t>(end - begin) > kMaxListedPlayers) {
        return {};
    }
    return std::vector<uint64_t>(begin, end);
}

// Reads one value of a member's lobby data.
//   matchmaking: the ISteamMatchmaking interface.
//   lobby:       the lobby's Steam ID.
//   member:      the member's Steam ID.
//   key:         the name of the value.
// Returns the value, or kUnknownValue if Steam doesn't have one yet.
std::string ReadMemberValue(void* matchmaking, uint64_t lobby, uint64_t member, const char* key) {
    auto getData = reinterpret_cast<GetLobbyMemberData>(steam_interfaces::VirtualFunction(
        matchmaking, game_addresses::kGetLobbyMemberDataSlot));
    const char* value = getData(matchmaking, lobby, member, key);
    return value == nullptr || value[0] == '\0' ? kUnknownValue : value;
}

// Converts the game's Opp value to the opponent's short tag, so that logs never show a
// full Steam ID.
//   value: the Steam ID as decimal text, "0" for no opponent, or kUnknownValue.
// Returns text to show in the log.
std::string DescribeOpponent(const std::string& value) {
    if (value == kUnknownValue) {
        return value;
    }
    uint64_t steamId = std::strtoull(value.c_str(), nullptr, 10);
    if (steamId == 0) {
        return "none";
    }
    char tag[8];
    std::snprintf(tag, sizeof(tag), "%04x", steam_interfaces::ShortTag(steamId));
    return tag;
}

// Reads a member's current status from Steam.
//   matchmaking: the ISteamMatchmaking interface.
//   lobby:       the lobby's Steam ID.
//   member:      the member's Steam ID.
// Returns the member's status.
MemberStatus ReadMemberStatus(void* matchmaking, uint64_t lobby, uint64_t member) {
    return {ReadMemberValue(matchmaking, lobby, member, "Ready"),
            ReadMemberValue(matchmaking, lobby, member, "InGame"),
            DescribeOpponent(ReadMemberValue(matchmaking, lobby, member, "Opp"))};
}

// Logs a new member, with their display name so that their tag can be matched to a name.
//   member: the member's Steam ID.
//   status: the member's first known status.
void LogNewMember(uint64_t member, const MemberStatus& status) {
    const char* name = "?";
    void* friends = steam_interfaces::FromContext(game_addresses::kFriendsContext);
    if (friends != nullptr) {
        auto getName = reinterpret_cast<GetFriendPersonaName>(steam_interfaces::VirtualFunction(
            friends, game_addresses::kGetFriendPersonaNameSlot));
        const char* steamName = getName(friends, member);
        name = steamName != nullptr ? steamName : name;
    }
    log::Writef("lobby", "Member %04x is %s: Ready=%s InGame=%s Opp=%s",
                steam_interfaces::ShortTag(member), name, status.ready.c_str(),
                status.inGame.c_str(), status.opponent.c_str());
}

// Logs one value of a member's status if it changed.
//   member:   the member's Steam ID.
//   key:      the name of the value.
//   previous: the last known value.
//   current:  the new value.
void LogIfChanged(uint64_t member, const char* key, const std::string& previous,
                  const std::string& current) {
    if (previous != current) {
        log::Writef("lobby", "Member %04x: %s=%s", steam_interfaces::ShortTag(member), key,
                    current.c_str());
    }
}

// Compares every member with their last known status and logs the differences.
//   matchmaking: the ISteamMatchmaking interface.
//   lobby:       the lobby's Steam ID.
//   members:     the game's current member list.
void UpdateMembers(void* matchmaking, uint64_t lobby, const std::vector<uint64_t>& members) {
    std::map<uint64_t, MemberStatus> current;
    for (uint64_t member : members) {
        MemberStatus status = ReadMemberStatus(matchmaking, lobby, member);
        auto known = g_members.find(member);
        if (known == g_members.end()) {
            LogNewMember(member, status);
        } else {
            LogIfChanged(member, "Ready", known->second.ready, status.ready);
            LogIfChanged(member, "InGame", known->second.inGame, status.inGame);
            LogIfChanged(member, "Opp", known->second.opponent, status.opponent);
        }
        current[member] = std::move(status);
    }
    for (const auto& [member, status] : g_members) {
        if (current.find(member) == current.end()) {
            log::Writef("lobby", "Member %04x left", steam_interfaces::ShortTag(member));
        }
    }
    g_members = std::move(current);
}

// Logs the list of spectators this player is expecting, if it changed.
//   spectators: the game's current list.
void UpdateExpectedSpectators(const std::vector<uint64_t>& spectators) {
    if (spectators == g_expectedSpectators) {
        return;
    }
    std::string tags;
    for (uint64_t spectator : spectators) {
        char tag[8];
        std::snprintf(tag, sizeof(tag), " %04x", steam_interfaces::ShortTag(spectator));
        tags += tag;
    }
    log::Writef("lobby", "Expecting spectators:%s", tags.empty() ? " none" : tags.c_str());
    g_expectedSpectators = spectators;
}

}  // namespace

namespace sgfixes::diagnostics {

void UpdateLobbyMonitor(uint64_t nowMilliseconds) {
    using namespace game_addresses;

    if (nowMilliseconds - g_lastPollMilliseconds < kPollMilliseconds) {
        return;
    }
    g_lastPollMilliseconds = nowMilliseconds;

    uintptr_t room = game_image::Rebase(kRoom);
    bool inRoom = *reinterpret_cast<const char*>(game_image::Rebase(kInRoomFlag)) != 0;
    uint64_t lobby = inRoom ? *reinterpret_cast<const uint64_t*>(room + kRoomLobbyIdOffset) : 0;
    void* matchmaking = lobby != 0 ? steam_interfaces::FromContext(kMatchmakingContext) : nullptr;

    if (matchmaking == nullptr) {
        g_members.clear();
        g_expectedSpectators.clear();
        return;
    }
    UpdateMembers(matchmaking, lobby, ReadSteamIdList(room, kRoomMembersBeginOffset,
                                                      kRoomMembersEndOffset));
    UpdateExpectedSpectators(ReadSteamIdList(room, kRoomExpectedSpectatorsBeginOffset,
                                             kRoomExpectedSpectatorsEndOffset));
}

}  // namespace sgfixes::diagnostics
