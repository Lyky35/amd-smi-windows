cmake_minimum_required(VERSION 3.20)
project(amd-smi-win VERSION 0.1.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set(ADLX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ADLX")

# ---------------------------------------------------------------------------
# Windows.h case shim.
#
# AMD's SDK does `#include <Windows.h>`. That resolves fine on Windows (the
# filesystem is case-insensitive) but fails when cross-compiling from a
# case-sensitive host, because mingw-w64 ships lowercase `windows.h`.
# Rather than patching vendored sources, synthesize a forwarding header.
# ---------------------------------------------------------------------------
set(ADLX_CASE_SHIM "${CMAKE_BINARY_DIR}/adlx-case-shim")
file(MAKE_DIRECTORY "${ADLX_CASE_SHIM}")
file(WRITE "${ADLX_CASE_SHIM}/Windows.h" "#pragma once\n#include <windows.h>\n")

# ---------------------------------------------------------------------------
# ADLX
#
# ADLX exposes no import library and no link-time symbols: ADLXHelper resolves
# amdadlx64.dll through LoadLibraryEx/GetProcAddress at startup. That means we
# compile only the header-only interface definitions plus two small platform
# shims, and the resulting .exe carries no ADLX dependency in its import table.
# ---------------------------------------------------------------------------
add_library(adlx STATIC
  ${ADLX_ROOT}/ADLXHelper/Windows/Cpp/ADLXHelper.cpp
  ${ADLX_ROOT}/Platform/Windows/WinAPIs.cpp
)

target_include_directories(adlx PUBLIC
  ${ADLX_CASE_SHIM}
  ${ADLX_ROOT}/Include
  ${ADLX_ROOT}/ADLXHelper/Windows/Cpp
  ${ADLX_ROOT}
)

# `__declspec(novtable)` is MSVC-only; clang warns and ignores it. The attribute
# only asks for vtable elision on interfaces ADLX instantiates internally, so
# ignoring it is semantically inert here.
target_compile_options(adlx PRIVATE -Wno-unknown-attributes)

target_link_libraries(adlx PUBLIC kernel32)

# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
add_executable(amd-smi
  src/main.cpp
  src/cli.cpp
  src/adlx.cpp
  src/box.cpp
  src/commands.cpp
  src/console.cpp
  src/layout.cpp
  src/default_view.cpp
  src/gpu_info.cpp
  src/output.cpp
  src/cmd_default.cpp
  src/cmd_static.cpp
  src/cmd_metric.cpp
)

target_link_libraries(amd-smi PRIVATE adlx)
target_compile_options(amd-smi PRIVATE -Wno-unknown-attributes)
target_compile_definitions(amd-smi PRIVATE "AMD_SMI_VERSION=\"${PROJECT_VERSION}\"")

install(TARGETS amd-smi RUNTIME DESTINATION bin)
