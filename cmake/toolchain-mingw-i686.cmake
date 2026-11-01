# Cross-compiles for 32-bit Windows from Linux using mingw-w64.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR i686)

set(CMAKE_C_COMPILER i686-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER i686-w64-mingw32-g++)
set(CMAKE_RC_COMPILER i686-w64-mingw32-windres)

# Windows test programs run through Wine.
set(CMAKE_CROSSCOMPILING_EMULATOR wine)
