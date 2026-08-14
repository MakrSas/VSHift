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
set(RPCS3_JIT_H
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rpcs3/Utilities/JIT.h")
set(RPCS3_SIMD_HPP
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rpcs3/rpcs3/util/simd.hpp")
set(RPCS3_ASM_HPP
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rpcs3/rpcs3/util/asm.hpp")
set(RPCS3_CELLSYSMODULE_CPP
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rpcs3/rpcs3/Emu/Cell/Modules/cellSysmodule.cpp")
set(RPCS3_CELLMIC_CPP
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rpcs3/rpcs3/Emu/Cell/Modules/cellMic.cpp")
set(RPCS3_CELLMIC_H
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rpcs3/rpcs3/Emu/Cell/Modules/cellMic.h")
set(RPCS3_THREAD_CPP
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rpcs3/Utilities/Thread.cpp")
set(RPCS3_SYSTEM_CPP
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rpcs3/rpcs3/Emu/System.cpp")
set(RPCS3_SPUTHREAD_CPP
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rpcs3/rpcs3/Emu/Cell/SPUThread.cpp")
set(RPCS3_JITLLVM_CPP
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rpcs3/Utilities/JITLLVM.cpp")
set(RPCS3_JITASM_CPP
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rpcs3/Utilities/JITASM.cpp")
set(RPCS3_PPUTHREAD_CPP
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rpcs3/rpcs3/Emu/Cell/PPUThread.cpp")
set(RPCS3_SPULLVM_CPP
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rpcs3/rpcs3/Emu/Cell/SPULLVMRecompiler.cpp")
set(RPCS3_SPUCOMMON_CPP
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rpcs3/rpcs3/Emu/Cell/SPUCommonRecompiler.cpp")
set(RPCS3_CUBEB_CMAKE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rpcs3/3rdparty/cubeb/cubeb/CMakeLists.txt")
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
    foreach(RPCS3_USB_SOURCE IN ITEMS
        "Cell/lv2/sys_usbd.cpp"
        "Io/Buzz.cpp"
        "Io/Dimensions.cpp"
        "Io/GameTablet.cpp"
        "Io/GHLtar.cpp"
        "Io/GunCon3.cpp"
        "Io/Infinity.cpp"
        "Io/KamenRider.cpp"
        "Io/LogitechG27.cpp"
        "Io/RB3MidiDrums.cpp"
        "Io/RB3MidiGuitar.cpp"
        "Io/RB3MidiKeyboard.cpp"
        "Io/Skylander.cpp"
        "Io/TopShotElite.cpp"
        "Io/TopShotFearmaster.cpp"
        "Io/Turntable.cpp"
        "Io/usb_device.cpp"
        "Io/usb_microphone.cpp"
        "Io/usb_vfs.cpp"
        "Io/usio.cpp")
        string(REPLACE "    ${RPCS3_USB_SOURCE}\n" "" RPCS3_EMU_TEXT "${RPCS3_EMU_TEXT}")
    endforeach()
    foreach(RPCS3_NETWORK_SOURCE IN ITEMS
        "NP/clans_client.cpp"
        "NP/np_requests.cpp"
        "Cell/Modules/sceNpClans.cpp")
        string(REPLACE "    ${RPCS3_NETWORK_SOURCE}\n" "" RPCS3_EMU_TEXT "${RPCS3_EMU_TEXT}")
    endforeach()
    string(REPLACE
        "    Cell/Modules/cellMic.cpp\n"
        ""
        RPCS3_EMU_TEXT "${RPCS3_EMU_TEXT}")
    # Video demuxing is not part of the first VSH boot boundary.  The module
    # pulls in the host media pipeline and uses C++23 ranges facilities that
    # are not available in Apple's device libc++ yet.  Keep it for the later
    # media/audio milestone instead of making the device core depend on it.
    string(REPLACE
        "    Cell/Modules/cellDmuxPamf.cpp\n"
        ""
        RPCS3_EMU_TEXT "${RPCS3_EMU_TEXT}")
    string(REPLACE
        "if(NOT ANDROID AND NOT APPLE)"
        "if(NOT VSHIFT_RPCS3_HEADLESS AND NOT APPLE)"
        RPCS3_EMU_TEXT "${RPCS3_EMU_TEXT}")
    file(WRITE "${RPCS3_EMU_CMAKE}" "${RPCS3_EMU_TEXT}")
endif()

# RPCS3's generic Apple guard calls the macOS-only write-protect API. iOS
# uses VSHift's signed executable-memory bridge instead; make the upstream
# guard a no-op on iOS while preserving the macOS implementation unchanged.
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    # The embedded cubeb project detects AudioUnit on iOS, then adds its
    # macOS CoreAudio run-loop helper.  That helper includes the macOS-only
    # CoreAudio/AudioHardware.h header.  VSHift intentionally uses the
    # headless audio backend during the firmware boot milestone, so keep the
    # cubeb API target but omit its host AudioUnit backend on iOS.
    file(READ "${RPCS3_CUBEB_CMAKE}" RPCS3_CUBEB_TEXT)
    if(NOT RPCS3_CUBEB_TEXT MATCHES "USE_AUDIOUNIT AND NOT CMAKE_SYSTEM_NAME")
        string(REPLACE
            "if(USE_AUDIOUNIT)"
            "if(USE_AUDIOUNIT AND NOT CMAKE_SYSTEM_NAME STREQUAL \"iOS\")"
            RPCS3_CUBEB_TEXT "${RPCS3_CUBEB_TEXT}")
        file(WRITE "${RPCS3_CUBEB_CMAKE}" "${RPCS3_CUBEB_TEXT}")
    endif()

    file(READ "${RPCS3_THREAD_CPP}" RPCS3_THREAD_TEXT)
    if(NOT RPCS3_THREAD_TEXT MATCHES "VSHIFT_RPCS3_IOS.*s_tls_is_attempting_recovery")
        string(REPLACE
            "#ifdef __APPLE__\n\tthread_local bool s_tls_is_attempting_recovery"
            "#if defined(__APPLE__) && !defined(VSHIFT_RPCS3_IOS)\n\tthread_local bool s_tls_is_attempting_recovery"
            RPCS3_THREAD_TEXT "${RPCS3_THREAD_TEXT}")
        file(WRITE "${RPCS3_THREAD_CPP}" "${RPCS3_THREAD_TEXT}")
    endif()

    file(READ "${RPCS3_SYSTEM_CPP}" RPCS3_SYSTEM_TEXT)
    if(NOT RPCS3_SYSTEM_TEXT MATCHES "VSHIFT_RPCS3_IOS.*Apple Silicon W\\^X")
        string(REPLACE
            "#ifdef __APPLE__\n\t\t\t\t// Apple Silicon W^X"
            "#if defined(__APPLE__) && !defined(VSHIFT_RPCS3_IOS)\n\t\t\t\t// Apple Silicon W^X"
            RPCS3_SYSTEM_TEXT "${RPCS3_SYSTEM_TEXT}")
        file(WRITE "${RPCS3_SYSTEM_CPP}" "${RPCS3_SYSTEM_TEXT}")
    endif()

    file(READ "${RPCS3_SPUTHREAD_CPP}" RPCS3_SPUTHREAD_TEXT)
    if(NOT RPCS3_SPUTHREAD_TEXT MATCHES "VSHIFT_RPCS3_IOS.*pthread_jit_write_protect_np\\(true\\)")
        string(REPLACE
            "#ifdef __APPLE__\n\tpthread_jit_write_protect_np(true)"
            "#if defined(__APPLE__) && !defined(VSHIFT_RPCS3_IOS)\n\tpthread_jit_write_protect_np(true)"
            RPCS3_SPUTHREAD_TEXT "${RPCS3_SPUTHREAD_TEXT}")
        file(WRITE "${RPCS3_SPUTHREAD_CPP}" "${RPCS3_SPUTHREAD_TEXT}")
    endif()

    file(READ "${RPCS3_JITLLVM_CPP}" RPCS3_JITLLVM_TEXT)
    string(REPLACE
        "#if defined(__APPLE__)\n\t\t\tpthread_jit_write_protect_np(false)"
        "#if defined(__APPLE__) && !defined(VSHIFT_RPCS3_IOS)\n\t\t\tpthread_jit_write_protect_np(false)"
        RPCS3_JITLLVM_TEXT "${RPCS3_JITLLVM_TEXT}")
    string(REPLACE
        "#if defined(__APPLE__)\n\t\t\tpthread_jit_write_protect_np(true)"
        "#if defined(__APPLE__) && !defined(VSHIFT_RPCS3_IOS)\n\t\t\tpthread_jit_write_protect_np(true)"
        RPCS3_JITLLVM_TEXT "${RPCS3_JITLLVM_TEXT}")
    file(WRITE "${RPCS3_JITLLVM_CPP}" "${RPCS3_JITLLVM_TEXT}")

    file(READ "${RPCS3_JITASM_CPP}" RPCS3_JITASM_TEXT)
    string(REPLACE
        "#ifdef __APPLE__\n\tpthread_jit_write_protect_np(false)"
        "#if defined(__APPLE__) && !defined(VSHIFT_RPCS3_IOS)\n\tpthread_jit_write_protect_np(false)"
        RPCS3_JITASM_TEXT "${RPCS3_JITASM_TEXT}")
    string(REPLACE
        "#ifdef __APPLE__\n\tpthread_jit_write_protect_np(true)"
        "#if defined(__APPLE__) && !defined(VSHIFT_RPCS3_IOS)\n\tpthread_jit_write_protect_np(true)"
        RPCS3_JITASM_TEXT "${RPCS3_JITASM_TEXT}")
    file(WRITE "${RPCS3_JITASM_CPP}" "${RPCS3_JITASM_TEXT}")

    file(READ "${RPCS3_PPUTHREAD_CPP}" RPCS3_PPUTHREAD_TEXT)
    string(REPLACE
        "#ifdef __APPLE__\n\t// Ensure correct state before executing JIT code"
        "#if defined(__APPLE__) && !defined(VSHIFT_RPCS3_IOS)\n\t// Ensure correct state before executing JIT code"
        RPCS3_PPUTHREAD_TEXT "${RPCS3_PPUTHREAD_TEXT}")
    string(REPLACE
        "#ifdef __APPLE__\n\t// Restore write-protection state (modified by build_function_asm)"
        "#if defined(__APPLE__) && !defined(VSHIFT_RPCS3_IOS)\n\t// Restore write-protection state (modified by build_function_asm)"
        RPCS3_PPUTHREAD_TEXT "${RPCS3_PPUTHREAD_TEXT}")
    string(REPLACE
        "#ifdef __APPLE__\n\tpthread_jit_write_protect_np(false)"
        "#if defined(__APPLE__) && !defined(VSHIFT_RPCS3_IOS)\n\tpthread_jit_write_protect_np(false)"
        RPCS3_PPUTHREAD_TEXT "${RPCS3_PPUTHREAD_TEXT}")
    string(REPLACE
        "#ifdef __APPLE__\n\t\tnamed_thread sym_worker"
        "#if defined(__APPLE__) && !defined(VSHIFT_RPCS3_IOS)\n\t\tnamed_thread sym_worker"
        RPCS3_PPUTHREAD_TEXT "${RPCS3_PPUTHREAD_TEXT}")
    string(REPLACE
        "#ifdef __APPLE__\n\t\t\t// Virtual memory mapped by MAP_JIT"
        "#if defined(__APPLE__) && !defined(VSHIFT_RPCS3_IOS)\n\t\t\t// Virtual memory mapped by MAP_JIT"
        RPCS3_PPUTHREAD_TEXT "${RPCS3_PPUTHREAD_TEXT}")
    string(REPLACE
        "#ifndef __APPLE__\n\t\t}"
        "#if !defined(__APPLE__) || defined(VSHIFT_RPCS3_IOS)\n\t\t}"
        RPCS3_PPUTHREAD_TEXT "${RPCS3_PPUTHREAD_TEXT}")
    file(WRITE "${RPCS3_PPUTHREAD_CPP}" "${RPCS3_PPUTHREAD_TEXT}")

    file(READ "${RPCS3_SPULLVM_CPP}" RPCS3_SPULLVM_TEXT)
    string(REPLACE
        "#if defined(__APPLE__)"
        "#if defined(__APPLE__) && !defined(VSHIFT_RPCS3_IOS)"
        RPCS3_SPULLVM_TEXT "${RPCS3_SPULLVM_TEXT}")
    file(WRITE "${RPCS3_SPULLVM_CPP}" "${RPCS3_SPULLVM_TEXT}")

    file(READ "${RPCS3_SPUCOMMON_CPP}" RPCS3_SPUCOMMON_TEXT)
    string(REPLACE
        "#if defined(__APPLE__)"
        "#if defined(__APPLE__) && !defined(VSHIFT_RPCS3_IOS)"
        RPCS3_SPUCOMMON_TEXT "${RPCS3_SPUCOMMON_TEXT}")
    file(WRITE "${RPCS3_SPUCOMMON_CPP}" "${RPCS3_SPUCOMMON_TEXT}")

    file(READ "${RPCS3_JIT_H}" RPCS3_JIT_TEXT)
    if(NOT RPCS3_JIT_TEXT MATCHES "VSHIFT_RPCS3_IOS")
        string(REPLACE
            "#ifdef __APPLE__\nstruct jit_write_guard"
            "#if defined(__APPLE__) && !defined(VSHIFT_RPCS3_IOS)\nstruct jit_write_guard"
            RPCS3_JIT_TEXT "${RPCS3_JIT_TEXT}")
        file(WRITE "${RPCS3_JIT_H}" "${RPCS3_JIT_TEXT}")
    endif()

    file(READ "${RPCS3_SIMD_HPP}" RPCS3_SIMD_TEXT)
    if(NOT RPCS3_SIMD_TEXT MATCHES "VSHIFT_RPCS3_IOS.*vqrdmlahq_s16")
        string(REPLACE
            "#if defined(ARCH_ARM64)\n\treturn vqrdmlahq_s16(c, a, b);"
            "#if defined(VSHIFT_RPCS3_IOS)\n\tv128 result{};\n\tfor (usz i = 0; i < 8; ++i)\n\t{\n\t\tconst s64 value = static_cast<s64>(a._s16[i]) * static_cast<s64>(b._s16[i]) * 2 + (static_cast<s64>(c._s16[i]) << 16) + 0x8000;\n\t\tresult._s16[i] = static_cast<s16>(std::clamp<s64>(value >> 16, -32768, 32767));\n\t}\n\treturn result;\n#elif defined(ARCH_ARM64)\n\treturn vqrdmlahq_s16(c, a, b);"
            RPCS3_SIMD_TEXT "${RPCS3_SIMD_TEXT}")
        file(WRITE "${RPCS3_SIMD_HPP}" "${RPCS3_SIMD_TEXT}")
    endif()

    file(READ "${RPCS3_ASM_HPP}" RPCS3_ASM_TEXT)
    if(NOT RPCS3_ASM_TEXT MATCHES "VSHIFT_RPCS3_IOS.*__atomic_fetch_or")
        string(REPLACE
            "#elif defined(ARCH_ARM64)\n\t\tu32 value = 0;\n\t\tu32* u32_ptr = static_cast<u32*>(ptr);\n\t\t__asm__ volatile(\"ldset %w0, %w0, %1\" : \"+r\"(value), \"=Q\"(*u32_ptr) : \"r\"(value));"
            "#elif defined(ARCH_ARM64)\n#if defined(VSHIFT_RPCS3_IOS)\n\t\t__atomic_fetch_or(static_cast<u32*>(ptr), 0u, __ATOMIC_RELAXED);\n#else\n\t\tu32 value = 0;\n\t\tu32* u32_ptr = static_cast<u32*>(ptr);\n\t\t__asm__ volatile(\"ldset %w0, %w0, %1\" : \"+r\"(value), \"=Q\"(*u32_ptr) : \"r\"(value));\n#endif"
            RPCS3_ASM_TEXT "${RPCS3_ASM_TEXT}")
        file(WRITE "${RPCS3_ASM_HPP}" "${RPCS3_ASM_TEXT}")
    endif()

    file(READ "${RPCS3_CELLSYSMODULE_CPP}" RPCS3_CELLSYSMODULE_TEXT)
    if(RPCS3_CELLSYSMODULE_TEXT MATCHES "std::ranges::contains")
        string(REPLACE
            "return std::ranges::contains(std::array{ \"BLUS30003\", \"BLES00035\", \"BLES00036\" }, std::string_view{ paramsfo.get_ptr() + 1, 9 });"
            "return std::ranges::any_of(std::array{ \"BLUS30003\", \"BLES00035\", \"BLES00036\" }, [&](const auto value) { return std::string_view{ value } == std::string_view{ paramsfo.get_ptr() + 1, 9 }; });"
            RPCS3_CELLSYSMODULE_TEXT "${RPCS3_CELLSYSMODULE_TEXT}")
        string(REPLACE
            "return std::ranges::contains(std::array{ \"BLJM60012\", \"BLES00039\", \"BLUS30027\", \"BLKS20001\" }, std::string_view{ paramsfo.get_ptr() + 1, 9 });"
            "return std::ranges::any_of(std::array{ \"BLJM60012\", \"BLES00039\", \"BLUS30027\", \"BLKS20001\" }, [&](const auto value) { return std::string_view{ value } == std::string_view{ paramsfo.get_ptr() + 1, 9 }; });"
            RPCS3_CELLSYSMODULE_TEXT "${RPCS3_CELLSYSMODULE_TEXT}")
        file(WRITE "${RPCS3_CELLSYSMODULE_CPP}" "${RPCS3_CELLSYSMODULE_TEXT}")
    endif()

    file(READ "${RPCS3_CELLMIC_H}" RPCS3_CELLMIC_TEXT)
    if(NOT RPCS3_CELLMIC_TEXT MATCHES "VSHIFT_RPCS3_NO_OPENAL")
        string(REPLACE
            "#include \"alc.h\""
            "#ifndef WITHOUT_OPENAL\n#include \"alc.h\"\n#else\nusing ALCdevice = void;\nusing ALCenum = int;\n#define VSHIFT_RPCS3_NO_OPENAL 1\n#endif"
            RPCS3_CELLMIC_TEXT "${RPCS3_CELLMIC_TEXT}")
        file(WRITE "${RPCS3_CELLMIC_H}" "${RPCS3_CELLMIC_TEXT}")
    endif()
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
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_compile_definitions(rpcs3_emu PUBLIC VSHIFT_RPCS3_IOS=1)
endif()
target_sources(rpcs3_emu PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rpcs3/rpcs3/rpcs3_version.cpp")
target_sources(rpcs3_emu PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/core/ps3/rpcs3_usb_stub.cpp")

add_library(vshift_ps3_core STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/core/ps3/rpcs3_core.cpp")
target_sources(vshift_ps3_core PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/core/ps3/firmware_installer.cpp")
target_include_directories(vshift_ps3_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(vshift_ps3_core PRIVATE VSHIFT_HAS_RPCS3_CORE=1)
target_link_libraries(vshift_ps3_core PUBLIC rpcs3_emu)
