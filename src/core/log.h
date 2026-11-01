// The mod's log file, stored in the sgfixes-logs folder next to the DLL.
//
// Each launch gets its own timestamped file, so a session can't be overwritten by the
// next one. Writing only adds a line to a queue, and a background thread does the file
// work. That keeps the game's main thread from ever waiting on the disk, and means the
// log keeps working if the main thread freezes. All functions are thread-safe.
#pragma once

#include <cstdint>

namespace sgfixes::log {

// Creates this session's log file and starts the writer thread. Lines written before
// this is called are ignored.
void Start();

// Queues one line.
//   source: short tag saying where the line came from, such as "mod" or "game".
//   text:   the line's text, without a trailing newline.
void Write(const char* source, const char* text);

// Queues one line built from a printf-style format.
//   source: short tag saying where the line came from.
//   format: printf format string.
//   ...:    the format's arguments.
void Writef(const char* source, const char* format, ...) __attribute__((format(printf, 2, 3)));

// Queues a message that may repeat often, unless its type has been logged too much lately.
//   source: short tag saying where the line came from.
//   key:    identifies the message type, as described in rate_limiter.h.
//   text:   the line's text, without a trailing newline.
void WriteLimited(const char* source, const void* key, const char* text);

// Waits long enough for queued lines to reach the file.
void Flush();

// Reads a steady clock, which isn't affected by changes to the system time. Use it for
// measuring durations.
// Returns the current time in milliseconds, from an arbitrary starting point.
uint64_t SteadyMilliseconds();

}  // namespace sgfixes::log
