# RPCS3 is kept as an intact upstream source graph. Platform adapters may
# disable host devices, but must not remove Cell/LV2/RSX translation units or
# replace module-table entries with link-only stubs.
set(VSHIFT_RPCS3_SOURCE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rpcs3"
    CACHE PATH "Path to the pinned RPCS3 source tree")

if(NOT EXISTS "${VSHIFT_RPCS3_SOURCE_DIR}/rpcs3/Emu/CMakeLists.txt")
    message(FATAL_ERROR
        "VSHIFT_BUILD_RPCS3_CORE requires third_party/rpcs3. "
        "Run: git submodule update --init --recursive")
endif()

enable_language(C)

# Keep the first milestone headless. Rendering is added through an RSX host
# adapter after the unchanged core links on the target platform.
set(USE_NATIVE_INSTRUCTIONS OFF CACHE BOOL "" FORCE)
set(USE_LTO OFF CACHE BOOL "" FORCE)
set(WITH_LLVM ${VSHIFT_RPCS3_WITH_LLVM} CACHE BOOL "" FORCE)
set(BUILD_LLVM OFF CACHE BOOL "" FORCE)
set(USE_VULKAN OFF CACHE BOOL "" FORCE)
set(USE_SDL OFF CACHE BOOL "" FORCE)
set(USE_FAUDIO OFF CACHE BOOL "" FORCE)
set(USE_LIBEVDEV OFF CACHE BOOL "" FORCE)
set(USE_SYSTEM_CUBEB OFF CACHE BOOL "" FORCE)
set(USE_SYSTEM_FFMPEG OFF CACHE BOOL "" FORCE)
set(BUILD_RPCS3_TESTS OFF CACHE BOOL "" FORCE)
set(USE_PRECOMPILED_HEADERS OFF CACHE BOOL "" FORCE)
set(WITH_LLVM OFF CACHE BOOL "" FORCE)
set(BUILD_LLVM OFF CACHE BOOL "" FORCE)
set(STATIC_LINK_LLVM OFF CACHE BOOL "" FORCE)
set(USE_SYSTEM_CURL OFF CACHE BOOL "" FORCE)
set(USE_SYSTEM_FAUDIO OFF CACHE BOOL "" FORCE)
set(USE_SYSTEM_GLSLANG OFF CACHE BOOL "" FORCE)
set(USE_SYSTEM_HIDAPI OFF CACHE BOOL "" FORCE)
set(USE_SYSTEM_LIBPNG OFF CACHE BOOL "" FORCE)
set(USE_SYSTEM_LIBUSB OFF CACHE BOOL "" FORCE)
set(USE_SYSTEM_MINIUPNPC OFF CACHE BOOL "" FORCE)
set(USE_SYSTEM_OPENAL OFF CACHE BOOL "" FORCE)
set(USE_SYSTEM_OPENCV OFF CACHE BOOL "" FORCE)
set(USE_SYSTEM_PUGIXML OFF CACHE BOOL "" FORCE)
set(USE_SYSTEM_RTMIDI OFF CACHE BOOL "" FORCE)
set(USE_SYSTEM_SDL OFF CACHE BOOL "" FORCE)
set(USE_SYSTEM_VULKAN_MEMORY_ALLOCATOR OFF CACHE BOOL "" FORCE)
set(USE_SYSTEM_WOLFSSL OFF CACHE BOOL "" FORCE)
set(USE_SYSTEM_ZLIB OFF CACHE BOOL "" FORCE)
set(USE_SYSTEM_ZSTD OFF CACHE BOOL "" FORCE)

# RPCS3's root normally generates this header before entering Emu.
include("${VSHIFT_RPCS3_SOURCE_DIR}/rpcs3/git-version.cmake")
set(VSHIFT_RPCS3_GENERATED_DIR "${CMAKE_BINARY_DIR}/rpcs3-generated")
file(MAKE_DIRECTORY "${VSHIFT_RPCS3_GENERATED_DIR}")
gen_git_version("${VSHIFT_RPCS3_GENERATED_DIR}")

add_subdirectory(
    "${VSHIFT_RPCS3_SOURCE_DIR}/3rdparty"
    "${CMAKE_BINARY_DIR}/rpcs3-3rdparty"
    EXCLUDE_FROM_ALL)

add_subdirectory(
    "${VSHIFT_RPCS3_SOURCE_DIR}/rpcs3/Emu"
    "${CMAKE_BINARY_DIR}/rpcs3-emu"
    EXCLUDE_FROM_ALL)

set_property(TARGET rpcs3_emu PROPERTY CXX_STANDARD 23)
set_property(TARGET rpcs3_emu PROPERTY CXX_STANDARD_REQUIRED ON)
target_include_directories(rpcs3_emu PUBLIC
    "${VSHIFT_RPCS3_GENERATED_DIR}"
    "${VSHIFT_RPCS3_SOURCE_DIR}"
    "${VSHIFT_RPCS3_SOURCE_DIR}/rpcs3"
    "${VSHIFT_RPCS3_SOURCE_DIR}/3rdparty")

add_library(vshift_rpcs3_headless STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/rpcs3_headless_glue.cpp"
    "${VSHIFT_RPCS3_SOURCE_DIR}/rpcs3/Emu/Io/pad_config.cpp"
    "${VSHIFT_RPCS3_SOURCE_DIR}/rpcs3/Input/product_info.cpp"
    "${VSHIFT_RPCS3_SOURCE_DIR}/rpcs3/Input/ps_move_config.cpp"
    "${VSHIFT_RPCS3_SOURCE_DIR}/rpcs3/Input/ps_move_tracker.cpp")
target_include_directories(vshift_rpcs3_headless PRIVATE
    "${VSHIFT_RPCS3_GENERATED_DIR}"
    "${VSHIFT_RPCS3_SOURCE_DIR}"
    "${VSHIFT_RPCS3_SOURCE_DIR}/rpcs3"
    "${VSHIFT_RPCS3_SOURCE_DIR}/3rdparty")
target_link_libraries(vshift_rpcs3_headless PRIVATE rpcs3_emu)

# PadHandler.cpp is part of RPCS3's Emu archive and uses the bundled Fusion
# IMU library.  Express that edge on the archive itself so CMake keeps Fusion
# after rpcs3_emu in the final static link order.
target_link_libraries(rpcs3_emu PUBLIC 3rdparty::fusion)

# Explicit milestone target: building it proves that the complete emulator
# module graph compiles and links before VSHift starts depending on it.
add_library(vshift_rpcs3_core INTERFACE)
target_link_libraries(vshift_rpcs3_core INTERFACE vshift_rpcs3_headless rpcs3_emu)
