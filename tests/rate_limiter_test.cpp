// Checks the rate limiter's behaviour. Exits with a non-zero code on failure.
#include <cstdio>

#include "core/rate_limiter.h"

namespace {

int g_failures = 0;

// Records a failure if a check doesn't hold.
//   condition:   the result of the check.
//   description: what was being checked, printed on failure.
void Expect(bool condition, const char* description) {
    if (!condition) {
        std::printf("FAILED: %s\n", description);
        g_failures++;
    }
}

}  // namespace

// Runs every check.
// Returns 0 if they all passed, or 1 otherwise.
int main() {
    const char keyA = 'a';
    const char keyB = 'b';

    {
        sgfixes::RateLimiter limiter(2, 1000);
        Expect(limiter.Check(&keyA, 10000).allow, "first line is allowed");
        Expect(limiter.Check(&keyA, 10001).allow, "second line is allowed");
        Expect(!limiter.Check(&keyA, 10002).allow, "third line is dropped");
        Expect(!limiter.Check(&keyA, 10003).allow, "fourth line is dropped");

        sgfixes::RateLimiter::Decision next = limiter.Check(&keyA, 11002);
        Expect(next.allow, "a new window allows lines again");
        Expect(next.suppressed == 2, "a new window reports the dropped lines");
    }
    {
        sgfixes::RateLimiter limiter(1, 1000);
        Expect(limiter.Check(&keyA, 10000).allow, "key A is allowed");
        Expect(!limiter.Check(&keyA, 10001).allow, "key A hits its limit");
        Expect(limiter.Check(&keyB, 10002).allow, "key B is counted separately");
    }

    if (g_failures == 0) {
        std::printf("All rate limiter tests passed\n");
    }
    return g_failures == 0 ? 0 : 1;
}
