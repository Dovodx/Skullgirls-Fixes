// Stops a repeating log message from flooding the log.
//
// For example, the game's lobby screen can change one setting 120 times a second. Each
// message type may log a limited number of lines per time window. Lines beyond that are
// counted, and the count is reported when the next window opens.
#pragma once

#include <cstdint>
#include <unordered_map>

namespace sgfixes {

class RateLimiter {
public:
    struct Decision {
        bool allow;           // Should this message be written?
        uint32_t suppressed;  // Messages of this type dropped in the previous window.
    };

    // Creates a limiter with no history.
    //   maxPerWindow:       how many lines of one message type to allow per window.
    //   windowMilliseconds: how long each window lasts.
    RateLimiter(uint32_t maxPerWindow, uint64_t windowMilliseconds)
        : maxPerWindow_(maxPerWindow), windowMilliseconds_(windowMilliseconds) {}

    // Decides whether a message may be written, and counts it either way.
    //   key:             identifies the message type. The game's format strings have a
    //                    fixed address, so that address works well as a key.
    //   nowMilliseconds: the current time from a steady clock.
    // Returns the decision, as described in the Decision struct.
    Decision Check(const void* key, uint64_t nowMilliseconds) {
        Window& window = windows_[key];

        uint32_t suppressed = 0;
        if (nowMilliseconds - window.startMilliseconds >= windowMilliseconds_) {
            suppressed = window.dropped;
            window = Window{nowMilliseconds, 0, 0};
        }
        if (window.written < maxPerWindow_) {
            window.written++;
            return {true, suppressed};
        }
        window.dropped++;
        return {false, suppressed};
    }

private:
    struct Window {
        uint64_t startMilliseconds = 0;
        uint32_t written = 0;
        uint32_t dropped = 0;
    };

    uint32_t maxPerWindow_;
    uint64_t windowMilliseconds_;
    std::unordered_map<const void*, Window> windows_;
};

}  // namespace sgfixes
