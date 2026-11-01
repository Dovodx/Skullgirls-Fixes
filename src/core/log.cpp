#include "core/log.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

#include "core/log_privacy.h"
#include "core/rate_limiter.h"

namespace sgfixes::log {
namespace {

constexpr DWORD kWriterIntervalMilliseconds = 100;
constexpr size_t kMaxQueuedLines = 8192;
constexpr size_t kMaxLineLength = 1024;

// Up to 20 lines of one message type every 5 seconds.
constexpr uint32_t kMaxLinesPerWindow = 20;
constexpr uint64_t kRateLimitWindowMilliseconds = 5000;

CRITICAL_SECTION g_lock;
bool g_started = false;
HANDLE g_file = INVALID_HANDLE_VALUE;
std::vector<std::string> g_queue;
size_t g_droppedLines = 0;
RateLimiter g_rateLimiter(kMaxLinesPerWindow, kRateLimitWindowMilliseconds);

// Finds the folder containing this DLL.
// Returns the folder's path, including the trailing slash.
std::wstring ModFolder() {
    HMODULE thisModule = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                           | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&ModFolder), &thisModule);
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(thisModule, path, MAX_PATH);
    std::wstring folder(path);
    return folder.substr(0, folder.find_last_of(L"\\/") + 1);
}

// Creates the sgfixes-logs folder if needed, then this session's timestamped log file.
// If that fails, g_file stays invalid and lines are discarded.
void OpenLogFile() {
    std::wstring folder = ModFolder() + L"sgfixes-logs";
    CreateDirectoryW(folder.c_str(), nullptr);

    SYSTEMTIME now;
    GetLocalTime(&now);
    wchar_t fileName[64];
    swprintf(fileName, 64, L"\\sgfixes-%04u%02u%02u-%02u%02u%02u.log", now.wYear, now.wMonth,
             now.wDay, now.wHour, now.wMinute, now.wSecond);

    g_file = CreateFileW((folder + fileName).c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                         CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
}

// Moves the queue to the file. The file write happens outside the lock, so that threads
// adding lines never wait for the disk.
void WriteQueueToFile() {
    std::vector<std::string> lines;
    size_t dropped;

    EnterCriticalSection(&g_lock);
    lines.swap(g_queue);
    dropped = g_droppedLines;
    g_droppedLines = 0;
    LeaveCriticalSection(&g_lock);

    if (g_file == INVALID_HANDLE_VALUE) {
        return;
    }
    if (dropped > 0) {
        lines.push_back("[log] queue was full, lost " + std::to_string(dropped) + " lines\n");
    }
    for (const std::string& line : lines) {
        DWORD written = 0;
        WriteFile(g_file, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
    }
}

// Background thread that writes queued lines to the file for the rest of the session.
// The unnamed parameter is the standard thread argument, which isn't used.
// Never returns.
DWORD WINAPI WriterThread(void*) {
    for (;;) {
        Sleep(kWriterIntervalMilliseconds);
        WriteQueueToFile();
    }
}

// Adds a timestamped line to the queue, or counts it as lost if the queue is full.
// The caller must hold g_lock.
//   source: short tag saying where the line came from.
//   text:   the line's text, without a trailing newline.
void QueueLine(const char* source, const char* text) {
    SYSTEMTIME now;
    GetLocalTime(&now);
    char prefix[48];
    std::snprintf(prefix, sizeof(prefix), "[%02u:%02u:%02u.%03u] [%s] ", now.wHour, now.wMinute,
                  now.wSecond, now.wMilliseconds, source);

    std::string line(prefix);
    line += log_privacy::RemovePrivateDetails(text);
    line += '\n';
    if (g_queue.size() < kMaxQueuedLines) {
        g_queue.push_back(std::move(line));
    } else {
        g_droppedLines++;
    }
}

}  // namespace

void Start() {
    InitializeCriticalSection(&g_lock);
    g_started = true;
    OpenLogFile();
    HANDLE thread = CreateThread(nullptr, 0, WriterThread, nullptr, 0, nullptr);
    if (thread != nullptr) {
        CloseHandle(thread);
    }
}

void Write(const char* source, const char* text) {
    if (!g_started) {
        return;
    }
    EnterCriticalSection(&g_lock);
    QueueLine(source, text);
    LeaveCriticalSection(&g_lock);
}

void Writef(const char* source, const char* format, ...) {
    char text[kMaxLineLength];
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(text, sizeof(text), format, arguments);
    va_end(arguments);
    Write(source, text);
}

void WriteLimited(const char* source, const void* key, const char* text) {
    if (!g_started) {
        return;
    }
    EnterCriticalSection(&g_lock);
    RateLimiter::Decision decision = g_rateLimiter.Check(key, SteadyMilliseconds());
    if (decision.suppressed > 0) {
        char note[96];
        std::snprintf(note, sizeof(note), "(%u more messages of the next type were not logged)",
                      decision.suppressed);
        QueueLine(source, note);
    }
    if (decision.allow) {
        QueueLine(source, text);
    }
    LeaveCriticalSection(&g_lock);
}

void Flush() {
    if (g_started) {
        // Twice the writer's interval is enough for it to finish a pass.
        Sleep(kWriterIntervalMilliseconds * 2);
    }
}

uint64_t SteadyMilliseconds() {
    static const uint64_t frequency = [] {
        LARGE_INTEGER value;
        QueryPerformanceFrequency(&value);
        return static_cast<uint64_t>(value.QuadPart);
    }();
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return static_cast<uint64_t>(counter.QuadPart) * 1000 / frequency;
}

}  // namespace sgfixes::log
