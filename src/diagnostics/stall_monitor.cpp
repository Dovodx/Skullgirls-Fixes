#include "diagnostics/stall_monitor.h"

#include <windows.h>

#include <atomic>
#include <cstdint>

#include "core/game_addresses.h"
#include "core/hooks.h"
#include "core/log.h"
#include "diagnostics/network_monitor.h"

namespace {

// A normal tick is under 20 ms, so anything over this is clearly a stall.
constexpr uint64_t kStallThresholdMilliseconds = 250;

// How long a freeze lasts before the watchdog reports it.
constexpr uint64_t kWatchdogThresholdMilliseconds = 1000;
constexpr DWORD kWatchdogIntervalMilliseconds = 100;

std::atomic<uint64_t> g_lastTickMilliseconds{0};

using MainLoopTick = void(__fastcall*)(void* ecx, void* edx);
MainLoopTick g_originalMainLoopTick = nullptr;

// Runs at the start of each main loop tick. It records long gaps between ticks, updates
// the network monitor, then runs the game's tick.
//   ecx, edx: the register contents at the call. The game calls this function with no
//             arguments, but declaring it fastcall means these registers pass through
//             unchanged, whatever values they hold.
void __fastcall OnMainLoopTick(void* ecx, void* edx) {
    uint64_t now = sgfixes::log::SteadyMilliseconds();
    uint64_t previous = g_lastTickMilliseconds.exchange(now);

    if (previous != 0 && now - previous > kStallThresholdMilliseconds) {
        sgfixes::log::Writef("stall", "Main thread paused for %u ms",
                             static_cast<unsigned>(now - previous));
    }
    sgfixes::diagnostics::UpdateNetworkMonitor(now);
    g_originalMainLoopTick(ecx, edx);
}

// Background thread that reports a freeze while it's still happening. It reports each
// freeze once. The unnamed parameter is the standard thread argument, which isn't used.
// Never returns.
DWORD WINAPI WatchdogThread(void*) {
    bool reported = false;
    for (;;) {
        Sleep(kWatchdogIntervalMilliseconds);
        uint64_t lastTick = g_lastTickMilliseconds.load();
        if (lastTick == 0) {
            continue;
        }
        uint64_t elapsed = sgfixes::log::SteadyMilliseconds() - lastTick;
        if (elapsed <= kWatchdogThresholdMilliseconds) {
            reported = false;
        } else if (!reported) {
            sgfixes::log::Writef("stall", "Main thread has been frozen for %u ms so far",
                                 static_cast<unsigned>(elapsed));
            reported = true;
        }
    }
}

}  // namespace

namespace sgfixes::diagnostics {

bool CreateStallMonitorHooks() {
    using namespace game_addresses;
    return hooks::Create("MainLoopTick", kMainLoopTick, kMainLoopTickBytes,
                         reinterpret_cast<void*>(OnMainLoopTick), &g_originalMainLoopTick);
}

void StartStallWatchdog() {
    HANDLE thread = CreateThread(nullptr, 0, WatchdogThread, nullptr, 0, nullptr);
    if (thread != nullptr) {
        CloseHandle(thread);
    }
}

}  // namespace sgfixes::diagnostics
