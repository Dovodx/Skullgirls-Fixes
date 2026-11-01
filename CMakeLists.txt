# Builds the mod as a 32-bit Windows DLL. See README.md for the build commands.
cmake_minimum_required(VERSION 3.20)
project(SkullgirlsFixes LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# MinHook is compiled directly into the mod, so there is no extra DLL to ship.
add_library(minhook STATIC
    external/minhook/src/buffer.c
    external/minhook/src/hook.c
    external/minhook/src/trampoline.c
    external/minhook/src/hde/hde32.c
)
target_include_directories(minhook PUBLIC external/minhook/include)

# The game loads this as Tolk.dll, the name of a library it already uses.
add_library(sgfixes SHARED
    src/core/game_image.cpp
    src/core/hooks.cpp
    src/core/log.cpp
    src/diagnostics/game_log_capture.cpp
    src/diagnostics/network_monitor.cpp
    src/diagnostics/stall_monitor.cpp
    src/proxy/tolk_proxy.cpp
    src/proxy/tolk.def
)
set_target_properties(sgfixes PROPERTIES PREFIX "" OUTPUT_NAME "Tolk")
target_include_directories(sgfixes PRIVATE src)
target_compile_options(sgfixes PRIVATE -Wall -Wextra -fno-exceptions -fno-rtti)

# Link the runtime statically so that the DLL needs nothing beyond Windows itself.
# Exceptions are off so that none can ever unwind through the game's own code.
target_link_options(sgfixes PRIVATE -static -static-libgcc -static-libstdc++ -s)
target_link_libraries(sgfixes PRIVATE minhook bcrypt)

enable_testing()
add_executable(rate_limiter_test tests/rate_limiter_test.cpp)
target_include_directories(rate_limiter_test PRIVATE src)
target_link_options(rate_limiter_test PRIVATE -static -static-libgcc -static-libstdc++)
add_test(NAME rate_limiter_test COMMAND rate_limiter_test)
