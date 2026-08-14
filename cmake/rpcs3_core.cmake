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

# VSHift embeds the platform-neutral RPCS3 emulator library directly.  This is
# intentionally a VSHift integration mode rather than a mobile-platform build:
# Qt, desktop graphics, host USB/HID, and host audio backends are supplied by
# adapters owned by VSHift when they are needed.
set(VSHIFT_RPCS3_HEADLESS ON CACHE BOOL "Build RPCS3 as a VSHift platform-neutral core" FORCE)

function(vshift_patch_rpcs3_cmake_between file_path begin_marker end_marker)
    string(JOIN "" replacement ${ARGN})
    file(READ "${file_path}" file_text)
    string(FIND "${file_text}" "${begin_marker}" begin_offset)
    string(FIND "${file_text}" "${end_marker}" end_offset)
    if(begin_offset EQUAL -1 OR end_offset EQUAL -1 OR end_offset LESS begin_offset)
        message(FATAL_ERROR
            "Could not patch RPCS3 CMake file '${file_path}' between '${begin_marker}' and '${end_marker}'")
    endif()
    string(SUBSTRING "${file_text}" 0 ${begin_offset} prefix)
    string(SUBSTRING "${file_text}" ${end_offset} -1 suffix)
    file(WRITE "${file_path}" "${prefix}${replacement}${suffix}")
endfunction()

# The upstream project owns the emulator sources, while this small configure
# patch keeps its dependency graph usable for a non-desktop host.  It is
# applied only in the checked-out submodule during configure and never changes
# the pinned RPCS3 commit in the VSHift repository.
set(RPCS3_3RDPARTY_CMAKE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rpcs3/3rdparty/CMakeLists.txt")
set(RPCS3_EMU_CMAKE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rpcs3/rpcs3/Emu/CMakeLists.txt")
file(READ "${RPCS3_3RDPARTY_CMAKE}" RPCS3_3RDPARTY_TEXT)
if(NOT RPCS3_3RDPARTY_TEXT MATCHES "VSHift headless integration")
    vshift_patch_rpcs3_cmake_between(
        "${RPCS3_3RDPARTY_CMAKE}" "# libusb" "# hidapi"
        "# VSHift headless integration: host USB passthrough is provided by a\n"
        "# later platform adapter, so the emulator core does not require a\n"
        "# desktop USB implementation during firmware boot.\n"
        "add_library(usb-1.0-static INTERFACE)\n"
        "add_library(usb-1.0-shared INTERFACE)\n\n")
    vshift_patch_rpcs3_cmake_between(
        "${RPCS3_3RDPARTY_CMAKE}" "# hidapi" "# glslang"
        "# VSHift headless integration: physical controllers are translated by\n"
        "# the frontend into the common input bridge.\n"
        "add_library(3rdparty_hidapi INTERFACE)\n\n")
    vshift_patch_rpcs3_cmake_between(
        "${RPCS3_3RDPARTY_CMAKE}" "# OpenAL" "# FAudio"
        "# VSHift headless integration: audio is routed through the common\n"
        "# frontend audio adapter instead of a desktop OpenAL device.\n"
        "add_library(3rdparty_openal INTERFACE)\n"
        "target_compile_definitions(3rdparty_openal INTERFACE WITHOUT_OPENAL=1)\n\n")
    vshift_patch_rpcs3_cmake_between(
        "${RPCS3_3RDPARTY_CMAKE}" "# FFMPEG" "# GLEW"
        "# VSHift headless integration: media decode is exposed through the\n"
        "# frontend media adapter; keep the core independent of a desktop\n"
        "# FFmpeg installation while the VSH boot path is brought up.\n"
        "add_library(3rdparty_ffmpeg INTERFACE)\n"
        "target_include_directories(3rdparty_ffmpeg SYSTEM INTERFACE\n"
        "    \"\${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg/include\")\n"
        "target_compile_definitions(3rdparty_ffmpeg INTERFACE VSHIFT_RPCS3_NO_HOST_MEDIA=1)\n\n")
    vshift_patch_rpcs3_cmake_between(
        "${RPCS3_3RDPARTY_CMAKE}" "# CURL" "# MINIUPNP"
        "# VSHift headless integration: network transport is not part of the\n"
        "# firmware boot boundary. A later frontend adapter can provide it.\n"
        "add_library(3rdparty_libcurl INTERFACE)\n\n")
    file(READ "${RPCS3_3RDPARTY_CMAKE}" RPCS3_3RDPARTY_TEXT)
    string(REPLACE
        "if (NOT ANDROID AND NOT APPLE)"
        "if (NOT VSHIFT_RPCS3_HEADLESS AND NOT APPLE)"
        RPCS3_3RDPARTY_TEXT "${RPCS3_3RDPARTY_TEXT}")
    string(REPLACE
        "if(NOT MSVC AND NOT ANDROID AND NOT APPLE)"
        "if(NOT MSVC AND NOT VSHIFT_RPCS3_HEADLESS AND NOT APPLE)"
        RPCS3_3RDPARTY_TEXT "${RPCS3_3RDPARTY_TEXT}")
    file(WRITE "${RPCS3_3RDPARTY_CMAKE}" "${RPCS3_3RDPARTY_TEXT}")

    file(READ "${RPCS3_EMU_CMAKE}" RPCS3_EMU_TEXT)
    string(REPLACE
        "if(NOT ANDROID AND NOT APPLE)"
        "if(NOT VSHIFT_RPCS3_HEADLESS AND NOT APPLE)"
        RPCS3_EMU_TEXT "${RPCS3_EMU_TEXT}")
    file(WRITE "${RPCS3_EMU_CMAKE}" "${RPCS3_EMU_TEXT}")
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

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rpcs3/3rdparty/libusb/libusb/libusb.h")
    target_include_directories(rpcs3_emu PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rpcs3/3rdparty/libusb/libusb")
endif()

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
