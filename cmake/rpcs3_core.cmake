if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rpcs3/rpcs3/Emu/CMakeLists.txt")
    message(FATAL_ERROR
        "VSHIFT_BUILD_PS3 requires the RPCS3 submodule. "
        "Run: git submodule update --init --recursive")
endif()

# RPCS3's CMake tree exposes its emulator implementation as rpcs3_emu.  The
# Qt application is deliberately not added here: VSHift owns the frontend and
# provides the iOS presentation/input/audio callbacks.
enable_language(C)

# The upstream root project sets this while entering its own CMake tree. We
# enter at the 3rdparty/Emu boundary instead, so establish the same policy
# version for dependencies which test it during configure.
cmake_minimum_required(VERSION 3.28)

# The upstream root normally generates this header after adding `Emu`. Keep
# generated files out of the submodule checkout while still compiling the
# version implementation required by System.cpp and Thread.cpp.
include("${CMAKE_CURRENT_SOURCE_DIR}/third_party/rpcs3/rpcs3/git-version.cmake")
set(RPCS3_GENERATED_DIR "${CMAKE_BINARY_DIR}/rpcs3-generated")
file(MAKE_DIRECTORY "${RPCS3_GENERATED_DIR}")
gen_git_version("${RPCS3_GENERATED_DIR}")

set(USE_NATIVE_INSTRUCTIONS OFF CACHE BOOL "" FORCE)
set(USE_LTO OFF CACHE BOOL "" FORCE)
set(USE_VULKAN OFF CACHE BOOL "" FORCE)
set(USE_SDL OFF CACHE BOOL "" FORCE)
set(USE_FAUDIO OFF CACHE BOOL "" FORCE)
set(USE_LIBEVDEV OFF CACHE BOOL "" FORCE)
set(USE_SYSTEM_CUBEB OFF CACHE BOOL "" FORCE)
set(USE_SYSTEM_FFMPEG OFF CACHE BOOL "" FORCE)
set(BUILD_RPCS3_TESTS OFF CACHE BOOL "" FORCE)
set(USE_PRECOMPILED_HEADERS OFF CACHE BOOL "" FORCE)

# The upstream tree uses this switch to omit Qt, desktop OpenGL, OpenAL, and
# FFmpeg-only frontend pieces.  It is also the closest existing headless build
# mode in RPCS3; the target still contains the real PPU/SPU, LV2/HLE, VFS,
# firmware loader, and RSX code.
set(ANDROID TRUE)
# RPCS3 uses the Android CMake switch to omit desktop-only dependencies. zstd
# also reads the Android API level while configuring, even for this headless
# reuse on Apple platforms.
set(ANDROID_PLATFORM_LEVEL 24 CACHE STRING "RPCS3 headless compatibility API" FORCE)

# The Android/headless branch of the current upstream CMake file does not
# create this optional target before declaring its public alias.  Supplying an
# interface target keeps that upstream build path intact without modifying the
# submodule.
if(NOT TARGET 3rdparty_ffmpeg)
    add_library(3rdparty_ffmpeg INTERFACE)
endif()

add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rpcs3/3rdparty"
    "${CMAKE_BINARY_DIR}/rpcs3-3rdparty"
    EXCLUDE_FROM_ALL)

# RPCS3's current core requires C++23. Keep the rest of VSHift on C++20 while
# giving the embedded upstream target the standard it declares in its root
# project.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rpcs3/rpcs3/Emu"
    "${CMAKE_BINARY_DIR}/rpcs3-emu"
    EXCLUDE_FROM_ALL)

set_property(TARGET rpcs3_emu PROPERTY CXX_STANDARD 23)
set_property(TARGET rpcs3_emu PROPERTY CXX_STANDARD_REQUIRED ON)

target_include_directories(rpcs3_emu PUBLIC
    "${RPCS3_GENERATED_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rpcs3"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rpcs3/rpcs3"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rpcs3/3rdparty")

target_compile_definitions(rpcs3_emu PUBLIC VSHIFT_RPCS3_HEADLESS=1)
target_sources(rpcs3_emu PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rpcs3/rpcs3/rpcs3_version.cpp")

add_library(vshift_ps3_core STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/core/ps3/rpcs3_core.cpp")
target_sources(vshift_ps3_core PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/core/ps3/firmware_installer.cpp")
target_include_directories(vshift_ps3_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(vshift_ps3_core PRIVATE VSHIFT_HAS_RPCS3_CORE=1)
target_link_libraries(vshift_ps3_core PUBLIC rpcs3_emu)
