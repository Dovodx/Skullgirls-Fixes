// Entry point of the mod.
//
// Tolk is a screen-reader library that ships in the game folder, and the game loads it
// by name. Replacing Tolk.dll is how the mod gets into the game: this file's location
// means no Wine DLL overrides are needed under Proton, and the mod doesn't compete with
// mods that use d3d9.dll or dbghelp.dll.
//
// The mod starts from Tolk_Load instead of DllMain. Windows holds a global lock while
// DllMain runs, which makes it unsafe for this kind of setup. The game calls Tolk_Load
// exactly once on its main thread, at the end of its window setup (FUN_00514e30), which
// is before it initialises Steam or reads its command line.

#include <windows.h>

#include "core/game_image.h"
#include "core/hooks.h"
#include "core/log.h"
#include "diagnostics/game_log_capture.h"
#include "diagnostics/network_monitor.h"
#include "diagnostics/stall_monitor.h"

namespace {

constexpr const char* kModVersion = "0.1.1-logging";

using TolkFunction = void(__cdecl*)();

// Calls a function in the real Tolk library, which the installer renames.
//   functionName: the exported name of a Tolk function with no arguments.
void CallOriginalTolk(const char* functionName) {
    HMODULE originalTolk = LoadLibraryA("Tolk_orig.dll");
    if (originalTolk == nullptr) {
        return;
    }
    auto function = reinterpret_cast<TolkFunction>(GetProcAddress(originalTolk, functionName));
    if (function != nullptr) {
        function();
    }
}

// Starts logging, checks the game build, then installs every hook. If anything fails,
// the game keeps running unmodified.
void StartMod() {
    sgfixes::log::Start();
    sgfixes::log::Writef("mod", "Skullgirls Fixes %s starting", kModVersion);

    // Every address the mod uses belongs to one exact build, so patching anything else
    // could corrupt the game. On a mismatch the game simply runs unmodified.
    if (!sgfixes::game_image::IsSupportedBuild()) {
        sgfixes::log::Write("mod", "Unsupported game build, so no hooks were installed");
        return;
    }
    sgfixes::log::Writef("mod", "Supported game build, loaded at 0x%08x",
                         static_cast<unsigned>(sgfixes::game_image::LoadedBase()));

    sgfixes::hooks::Begin();
    bool created = sgfixes::diagnostics::CreateGameLogCaptureHooks()
                   && sgfixes::diagnostics::CreateNetworkMonitorHooks()
                   && sgfixes::diagnostics::CreateStallMonitorHooks();
    if (created && sgfixes::hooks::EnableAll()) {
        sgfixes::diagnostics::StartStallWatchdog();
        sgfixes::log::Write("mod", "All hooks installed");
    } else {
        sgfixes::hooks::RemoveAll();
        sgfixes::log::Write("mod", "Hook installation failed, so the game is running unmodified");
    }
}

}  // namespace

// Replaces Tolk's load function. The game calls this during startup.
extern "C" __declspec(dllexport) void __cdecl Tolk_Load() {
    // Run the real function first, so that the library works even if the mod fails.
    CallOriginalTolk("Tolk_Load");
    StartMod();
}

// Replaces Tolk's unload function. The game calls this at shutdown, which gives the log
// a chance to reach the file.
extern "C" __declspec(dllexport) void __cdecl Tolk_Unload() {
    sgfixes::log::Write("mod", "Game is shutting down");
    sgfixes::log::Flush();
    CallOriginalTolk("Tolk_Unload");
}
