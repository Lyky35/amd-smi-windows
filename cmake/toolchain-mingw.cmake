# Cross-compile toolchain: Linux host -> Windows x86-64 target.
# Works with both llvm-mingw and GCC mingw-w64 by pointing MINGW_PREFIX
# at the respective toolchain root.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(MINGW_PREFIX "/tmp/opencode/llvm-mingw" CACHE PATH "MinGW-w64 toolchain root")

set(CMAKE_C_COMPILER   "${MINGW_PREFIX}/bin/x86_64-w64-mingw32-clang")
set(CMAKE_CXX_COMPILER "${MINGW_PREFIX}/bin/x86_64-w64-mingw32-clang++")
set(CMAKE_RC_COMPILER  "${MINGW_PREFIX}/bin/x86_64-w64-mingw32-windres")

set(CMAKE_FIND_ROOT_PATH "${MINGW_PREFIX}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# We ship a bare .exe; no MinGW runtime DLLs are required because ADLX is
# resolved at runtime via LoadLibraryEx and everything else is Win32.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static-libgcc -static-libstdc++")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM_BEFORE  NEVER)
