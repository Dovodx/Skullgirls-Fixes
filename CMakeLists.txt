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
    src/core/steam_interfaces.cpp
    src/diagnostics/game_log_capture.cpp
    src/diagnostics/lobby_monitor.cpp
    src/diagnostics/network_monitor.cpp
    src/diagnostics/session_monitor.cpp
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

# Builds the download, build/skullgirls-fixes.zip. Everything in it sits in one sgfixes
# folder, so extracting it into the game folder can't overwrite any of the game's files.
# MinHook is compiled into the DLL, so its license has to ship with it.
set(PACKAGE_DIR ${CMAKE_BINARY_DIR}/package/sgfixes)
add_custom_target(release_zip
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/package
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PACKAGE_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:sgfixes> install/install.bat
            install/uninstall.bat install/install.sh install/uninstall.sh README.md ${PACKAGE_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy LICENSE ${PACKAGE_DIR}/LICENSE.txt
    COMMAND ${CMAKE_COMMAND} -E copy external/minhook/LICENSE.txt ${PACKAGE_DIR}/MinHook-LICENSE.txt
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR}/package
            ${CMAKE_COMMAND} -E tar cf ../skullgirls-fixes.zip --format=zip sgfixes
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS sgfixes
    VERBATIM
)

enable_testing()
add_executable(rate_limiter_test tests/rate_limiter_test.cpp)
target_include_directories(rate_limiter_test PRIVATE src)
target_link_options(rate_limiter_test PRIVATE -static -static-libgcc -static-libstdc++)
add_test(NAME rate_limiter_test COMMAND rate_limiter_test)

add_executable(log_privacy_test tests/log_privacy_test.cpp)
target_include_directories(log_privacy_test PRIVATE src)
target_link_options(log_privacy_test PRIVATE -static -static-libgcc -static-libstdc++)
add_test(NAME log_privacy_test COMMAND log_privacy_test)
