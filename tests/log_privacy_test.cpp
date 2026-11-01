// Checks that personal details are removed from log lines. Exits with a non-zero code on
// failure.
#include <cstdio>
#include <string>

#include "core/log_privacy.h"

namespace {

int g_failures = 0;

// Records a failure if cleaning a line doesn't give the expected result.
//   input:    the original line.
//   expected: what the line should become.
void ExpectCleaned(const std::string& input, const std::string& expected) {
    std::string actual = sgfixes::log_privacy::RemovePrivateDetails(input);
    if (actual != expected) {
        std::printf("FAILED: \"%s\" became \"%s\"\n", input.c_str(), actual.c_str());
        g_failures++;
    }
}

}  // namespace

// Runs every check.
// Returns 0 if they all passed, or 1 otherwise.
int main() {
    // Example IDs, not real accounts.
    ExpectCleaned("Save\\76561190000000001\\SAVEDATA.BIN", "Save\\<steam-id>\\SAVEDATA.BIN");
    ExpectCleaned("a 76561190000000001 b 76561190000000002", "a <steam-id> b <steam-id>");

    // Numbers that only resemble an ID must survive.
    ExpectCleaned("seed 7656119", "seed 7656119");
    ExpectCleaned("id 765611900000000012", "id 765611900000000012");
    ExpectCleaned("id 12345678901234567", "id 12345678901234567");

    ExpectCleaned("Z:\\home\\alice\\.local\\share\\Steam",
                  "Z:\\home\\<user>\\.local\\share\\Steam");
    ExpectCleaned("/home/alice/game", "/home/<user>/game");
    ExpectCleaned("C:\\Users\\Alice Smith\\Documents\\x", "C:\\Users\\<user>\\Documents\\x");
    ExpectCleaned("C:\\users\\steamuser\\Save Data\\76561190000000001\\file",
                  "C:\\users\\<user>\\Save Data\\<steam-id>\\file");

    // Ordinary text must survive.
    ExpectCleaned("[RCS] Sending packet 10 to remote; ack is 9",
                  "[RCS] Sending packet 10 to remote; ack is 9");
    ExpectCleaned("went home/users", "went home/users");
    ExpectCleaned("", "");

    if (g_failures == 0) {
        std::printf("All log privacy tests passed\n");
    }
    return g_failures == 0 ? 0 : 1;
}
