#include "diagnostics/game_log_capture.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "core/game_addresses.h"
#include "core/hooks.h"
#include "core/log.h"

// The three log functions take a variable number of arguments, and C++ can't forward
// those to another function. So each hook is a short assembly stub that passes a pointer
// to the caller's arguments to a handler, then jumps to the original function with the
// stack and arguments untouched. The game's function runs exactly as it would have.
//
// These functions use the cdecl convention, so EAX, ECX and EDX are free to use.
//   stubName:     name of the stub function to define.
//   handlerName:  C-linkage function that receives a pointer to the caller's arguments.
//   originalName: C-linkage variable holding the address of the game's original code.
#define DEFINE_LOG_STUB(stubName, handlerName, originalName)      \
    __attribute__((naked)) static void stubName() {               \
        asm("leal 4(%esp), %eax\n"                                \
            "pushl %eax\n"                                        \
            "call _" #handlerName "\n"                            \
            "addl $4, %esp\n"                                     \
            "jmp *_" #originalName "\n");                         \
    }

// Stack layout of each function, as seen by the handlers:
//   GameLog:    [0] Logger*, [1] format, [2...] format arguments
//   DebugPrint: [0] format, [1...] format arguments
//   GgpoLog:    [0] format, [1...] format arguments
extern "C" {

void* g_originalGameLog = nullptr;
void* g_originalDebugPrint = nullptr;
void* g_originalGgpoLog = nullptr;

void OnGameLog(uintptr_t* arguments);
void OnDebugPrint(uintptr_t* arguments);
void OnGgpoLog(uintptr_t* arguments);

}

DEFINE_LOG_STUB(GameLogStub, OnGameLog, g_originalGameLog)
DEFINE_LOG_STUB(DebugPrintStub, OnDebugPrint, g_originalDebugPrint)
DEFINE_LOG_STUB(GgpoLogStub, OnGgpoLog, g_originalGgpoLog)

namespace {

constexpr size_t kMaxMessageLength = 1024;

// How many raw arguments to show for a debug print.
constexpr int kDebugArgumentCount = 4;

// Game log messages that add nothing to network diagnostics. These cover save files and
// package mounting, which also make up most of the log's file paths.
constexpr const char* kIgnoredGameLogPrefixes[] = {"-- ", "---- ", "Mounting pkg",
                                                   "Beginning Save Process"};

// Debug prints that add nothing. The game's normal logger sends each of its lines through
// the debug print as a bare "%s", so those duplicate the [game] lines. The second message
// is printed for every hit landed in a fight, and the third while files are loading.
constexpr const char* kIgnoredDebugMessages[] = {"%s", "GUID %u hit GUID %u on frame %d",
                                                 "cache %s"};

// GGPO logs constantly while active, so only messages about connection trouble are kept.
constexpr const char* kGgpoKeywords[] = {"Failure", "Disconnect", "disconnect", "stopped receiving",
                                         "Blowing out", "interrupted", "resumed"};

// Removes line endings from a message, because the log adds its own.
//   text: the message, which is modified in place.
void RemoveTrailingNewlines(char* text) {
    size_t length = std::strlen(text);
    while (length > 0 && (text[length - 1] == '\n' || text[length - 1] == '\r')) {
        text[--length] = '\0';
    }
}

// Formats a game message and writes it to the log, subject to rate limiting.
//   source:          short tag saying where the line came from.
//   format:          the game's printf format string. Also used as the rate-limit key.
//   formatArguments: the first format argument on the caller's stack.
void WriteFormatted(const char* source, const char* format, uintptr_t* formatArguments) {
    char message[kMaxMessageLength];
    // On 32-bit x86, a va_list is simply a pointer to the arguments on the stack. The
    // mingw version is used because it understands every format the game uses, such as %zu.
    __mingw_vsnprintf(message, sizeof(message), format, reinterpret_cast<va_list>(formatArguments));
    RemoveTrailingNewlines(message);
    sgfixes::log::WriteLimited(source, format, message);
}

// Checks whether a game log message should be left out of the log.
//   format: the message's format string.
// Returns true if it starts with one of kIgnoredGameLogPrefixes.
bool IsIgnoredGameLogMessage(const char* format) {
    for (const char* prefix : kIgnoredGameLogPrefixes) {
        if (std::strncmp(format, prefix, std::strlen(prefix)) == 0) {
            return true;
        }
    }
    return false;
}

// Checks whether a debug print should be left out of the log.
//   message: the format string, with its line endings removed.
// Returns true if it matches one of kIgnoredDebugMessages.
bool IsIgnoredDebugMessage(const char* message) {
    for (const char* ignored : kIgnoredDebugMessages) {
        if (std::strcmp(message, ignored) == 0) {
            return true;
        }
    }
    return false;
}

// Checks whether a GGPO message is worth keeping.
//   format: the message's format string.
// Returns true if it contains one of kGgpoKeywords.
bool MentionsConnectionTrouble(const char* format) {
    for (const char* keyword : kGgpoKeywords) {
        if (std::strstr(format, keyword) != nullptr) {
            return true;
        }
    }
    return false;
}

}  // namespace

// Receives each call to the game's normal logger, including calls the game itself
// ignores because -log is off.
//   arguments: the caller's stack arguments, laid out as described above.
extern "C" void OnGameLog(uintptr_t* arguments) {
    const char* format = reinterpret_cast<const char*>(arguments[1]);
    if (!IsIgnoredGameLogMessage(format)) {
        WriteFormatted("game", format, &arguments[2]);
    }
}

// Debug prints are normally never formatted, so a mistake in one of the game's format
// strings could have gone unnoticed, and formatting it here could crash the game. So
// this writes the format string as-is, followed by the raw argument values.
//   arguments: the caller's stack arguments, laid out as described above.
extern "C" void OnDebugPrint(uintptr_t* arguments) {
    const char* format = reinterpret_cast<const char*>(arguments[0]);

    char message[kMaxMessageLength];
    std::snprintf(message, sizeof(message), "%s", format);
    RemoveTrailingNewlines(message);
    if (IsIgnoredDebugMessage(message)) {
        return;
    }

    if (std::strchr(format, '%') != nullptr) {
        char values[64] = " |";
        for (int i = 1; i <= kDebugArgumentCount; i++) {
            size_t used = std::strlen(values);
            std::snprintf(values + used, sizeof(values) - used, " %08x",
                          static_cast<unsigned>(arguments[i]));
        }
        std::strncat(message, values, sizeof(message) - std::strlen(message) - 1);
    }
    sgfixes::log::WriteLimited("debug", format, message);
}

// Receives each call to GGPO's logger, and keeps only messages about connection trouble.
//   arguments: the caller's stack arguments, laid out as described above.
extern "C" void OnGgpoLog(uintptr_t* arguments) {
    const char* format = reinterpret_cast<const char*>(arguments[0]);
    if (MentionsConnectionTrouble(format)) {
        WriteFormatted("ggpo", format, &arguments[1]);
    }
}

namespace sgfixes::diagnostics {

bool CreateGameLogCaptureHooks() {
    using namespace game_addresses;
    return hooks::Create("GameLog", kGameLog, kGameLogBytes,
                         reinterpret_cast<void*>(GameLogStub), &g_originalGameLog)
           && hooks::Create("DebugPrint", kDebugPrint, kDebugPrintBytes,
                            reinterpret_cast<void*>(DebugPrintStub), &g_originalDebugPrint)
           && hooks::Create("GgpoLog", kGgpoLog, kGgpoLogBytes,
                            reinterpret_cast<void*>(GgpoLogStub), &g_originalGgpoLog);
}

}  // namespace sgfixes::diagnostics
