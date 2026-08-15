cmake_minimum_required(VERSION 3.24)

project(VSHift VERSION 0.1.0 LANGUAGES CXX)

find_package(ZLIB REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(VSHIFT_BUILD_TESTS "Build host-side tests" ON)
option(VSHIFT_BUILD_IOS "Build the iOS application target" OFF)
option(VSHIFT_BUILD_RPCS3_CORE "Build the pinned, intact RPCS3 emulator core" OFF)
option(VSHIFT_RPCS3_WITH_LLVM
    "Enable LLVM PPU/SPU recompilation in the RPCS3 core build" OFF)
option(VSHIFT_RPCS3_WITH_VULKAN
    "Build the RPCS3 RSX Vulkan backend (requires a target Vulkan/MoltenVK SDK)" OFF)

if(VSHIFT_BUILD_RPCS3_CORE)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/rpcs3_core.cmake)
endif()

add_library(vshift_core STATIC
    core/crypto/aes.cpp
    core/boot/ps4_boot.cpp
    core/boot/ps3_runtime.cpp
    core/boot/synthetic_boot.cpp
    core/cpu/executable_memory.cpp
    core/cpu/arm64_jit.cpp
    core/cpu/ppu_runtime.cpp
    core/cpu/ir.cpp
    core/cpu/x86_runtime.cpp
    core/cpu/x86_decoder.cpp
    core/memory/guest_memory.cpp
    core/loader/elf.cpp
    core/loader/elf_dynamic.cpp
    core/loader/elf_loader.cpp
    core/loader/self.cpp
    core/loader/self_loader.cpp
    core/loader/ps3_sce.cpp
    core/loader/ps3_self.cpp
    core/firmware/catalog.cpp
    core/firmware/vfs.cpp
    core/firmware/slb2.cpp
    core/firmware/pup.cpp
    core/firmware/ps3_pup.cpp
    core/firmware/ps3_package.cpp
    core/firmware/ps3_tar.cpp
    core/hle/kernel.cpp
    core/hle/ps3_lv2.cpp
    core/video/framebuffer.cpp
)

target_include_directories(vshift_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(vshift_core PUBLIC ZLIB::ZLIB)

target_compile_features(vshift_core PUBLIC cxx_std_20)

if(APPLE)
    target_compile_definitions(vshift_core PRIVATE VSHIFT_APPLE=1)
endif()

# Stable boundaries for the application and independently-developed console
# cores. The existing vshift_core remains the PS3 implementation layer during
# this incremental migration; consumers should depend on the PS3Core wrapper.
add_library(vshift_core_api INTERFACE)
target_include_directories(vshift_core_api INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(vshift_platform INTERFACE)
target_include_directories(vshift_platform INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(vshift_ps3_core STATIC
    cores/ps3/ps3_core.cpp
)
target_include_directories(vshift_ps3_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(vshift_ps3_core PUBLIC
    vshift_core
    vshift_core_api
    vshift_platform
)

add_executable(vshift_ps3_firmware_extract
    tools/ps3_firmware_extract.cpp
)
target_link_libraries(vshift_ps3_firmware_extract PRIVATE vshift_ps3_core)
if(VSHIFT_BUILD_RPCS3_CORE)
    target_link_libraries(vshift_ps3_firmware_extract PRIVATE vshift_rpcs3_core)
    target_compile_definitions(vshift_ps3_firmware_extract PRIVATE VSHIFT_HAVE_RPCS3_DECRYPT=1)
endif()

if(VSHIFT_BUILD_RPCS3_CORE)
    add_executable(vshift_rpcs3_boot_probe
        tools/rpcs3_boot_probe.cpp
    )
    target_link_libraries(vshift_rpcs3_boot_probe PRIVATE
        vshift_rpcs3_core
    )
endif()

if(VSHIFT_BUILD_TESTS)
    enable_testing()

    add_executable(vshift_jit_smoke
        tests/jit_smoke.cpp
    )
    target_link_libraries(vshift_jit_smoke PRIVATE vshift_core)

    add_executable(vshift_ir_test
        tests/ir_test.cpp
    )
    target_link_libraries(vshift_ir_test PRIVATE vshift_core)

    add_executable(vshift_differential_test
        tests/differential_test.cpp
    )
    target_link_libraries(vshift_differential_test PRIVATE vshift_core)

    add_executable(vshift_synthetic_boot_test
        tests/synthetic_boot_test.cpp
    )
    target_link_libraries(vshift_synthetic_boot_test PRIVATE vshift_core)

    add_executable(vshift_slb2_test
        tests/slb2_test.cpp
    )
    target_link_libraries(vshift_slb2_test PRIVATE vshift_core)

    add_executable(vshift_pup_test
        tests/pup_test.cpp
    )
    target_link_libraries(vshift_pup_test PRIVATE vshift_core)

    add_executable(vshift_ps3_pup_test
        tests/ps3_pup_test.cpp
    )
    target_link_libraries(vshift_ps3_pup_test PRIVATE vshift_core)

    add_executable(vshift_ps3_sce_test
        tests/ps3_sce_test.cpp
    )
    target_link_libraries(vshift_ps3_sce_test PRIVATE vshift_core)

    add_executable(vshift_ps3_self_test
        tests/ps3_self_test.cpp
    )
    target_link_libraries(vshift_ps3_self_test PRIVATE vshift_core)

    add_executable(vshift_ppu_runtime_test
        tests/ppu_runtime_test.cpp
    )
    target_link_libraries(vshift_ppu_runtime_test PRIVATE vshift_core)

    add_executable(vshift_ps3_lv2_test
        tests/ps3_lv2_test.cpp
    )
    target_link_libraries(vshift_ps3_lv2_test PRIVATE vshift_core)

    add_executable(vshift_ps3_crypto_test
        tests/ps3_crypto_test.cpp
    )
    target_link_libraries(vshift_ps3_crypto_test PRIVATE vshift_core)

    add_executable(vshift_ps3_core_test
        tests/ps3_core_test.cpp
    )
    target_link_libraries(vshift_ps3_core_test PRIVATE vshift_ps3_core)

    add_executable(vshift_catalog_test
        tests/catalog_test.cpp
    )
    target_link_libraries(vshift_catalog_test PRIVATE vshift_core)

    add_executable(vshift_elf_test
        tests/elf_test.cpp
    )
    target_link_libraries(vshift_elf_test PRIVATE vshift_core)

    add_executable(vshift_guest_memory_test
        tests/guest_memory_test.cpp
    )
    target_link_libraries(vshift_guest_memory_test PRIVATE vshift_core)

    add_executable(vshift_elf_loader_test
        tests/elf_loader_test.cpp
    )
    target_link_libraries(vshift_elf_loader_test PRIVATE vshift_core)

    add_executable(vshift_elf_dynamic_test
        tests/elf_dynamic_test.cpp
    )
    target_link_libraries(vshift_elf_dynamic_test PRIVATE vshift_core)

    add_executable(vshift_self_test
        tests/self_test.cpp
    )
    target_link_libraries(vshift_self_test PRIVATE vshift_core)

    add_executable(vshift_ps4_boot_test
        tests/ps4_boot_test.cpp
    )
    target_link_libraries(vshift_ps4_boot_test PRIVATE vshift_core)

    add_executable(vshift_x86_runtime_test
        tests/x86_runtime_test.cpp
    )
    target_link_libraries(vshift_x86_runtime_test PRIVATE vshift_core)

    add_executable(vshift_vfs_test
        tests/vfs_test.cpp
    )
    target_link_libraries(vshift_vfs_test PRIVATE vshift_core)

    add_executable(vshift_framebuffer_test
        tests/framebuffer_test.cpp
    )
    target_link_libraries(vshift_framebuffer_test PRIVATE vshift_core)

    add_executable(vshift_hle_test
        tests/hle_test.cpp
    )
    target_link_libraries(vshift_hle_test PRIVATE vshift_core)

    add_test(NAME vshift_jit_smoke COMMAND vshift_jit_smoke)
    add_test(NAME vshift_ir_test COMMAND vshift_ir_test)
    add_test(NAME vshift_differential_test COMMAND vshift_differential_test)
    add_test(NAME vshift_synthetic_boot_test COMMAND vshift_synthetic_boot_test)
    add_test(NAME vshift_slb2_test COMMAND vshift_slb2_test)
    add_test(NAME vshift_pup_test COMMAND vshift_pup_test)
    add_test(NAME vshift_ps3_pup_test COMMAND vshift_ps3_pup_test)
    add_test(NAME vshift_ps3_sce_test COMMAND vshift_ps3_sce_test)
    add_test(NAME vshift_ps3_self_test COMMAND vshift_ps3_self_test)
    add_test(NAME vshift_ppu_runtime_test COMMAND vshift_ppu_runtime_test)
    add_test(NAME vshift_ps3_lv2_test COMMAND vshift_ps3_lv2_test)
    add_test(NAME vshift_ps3_crypto_test COMMAND vshift_ps3_crypto_test)
    add_test(NAME vshift_ps3_core_test COMMAND vshift_ps3_core_test)
    add_test(NAME vshift_catalog_test COMMAND vshift_catalog_test)
    add_test(NAME vshift_elf_test COMMAND vshift_elf_test)
    add_test(NAME vshift_guest_memory_test COMMAND vshift_guest_memory_test)
    add_test(NAME vshift_elf_loader_test COMMAND vshift_elf_loader_test)
    add_test(NAME vshift_elf_dynamic_test COMMAND vshift_elf_dynamic_test)
    add_test(NAME vshift_self_test COMMAND vshift_self_test)
    add_test(NAME vshift_ps4_boot_test COMMAND vshift_ps4_boot_test)
    add_test(NAME vshift_x86_runtime_test COMMAND vshift_x86_runtime_test)
    add_test(NAME vshift_vfs_test COMMAND vshift_vfs_test)
    add_test(NAME vshift_framebuffer_test COMMAND vshift_framebuffer_test)
    add_test(NAME vshift_hle_test COMMAND vshift_hle_test)
endif()

add_executable(vshift_firmware_inspect
    tools/firmware_inspect.cpp
)
target_link_libraries(vshift_firmware_inspect PRIVATE vshift_core)

add_executable(vshift_ps3_firmware_inspect
    tools/ps3_firmware_inspect.cpp
)
target_link_libraries(vshift_ps3_firmware_inspect PRIVATE vshift_core)

add_executable(vshift_ps3_vsh_bootstrap_inspect
    tools/ps3_vsh_bootstrap_inspect.cpp
)
target_link_libraries(vshift_ps3_vsh_bootstrap_inspect PRIVATE vshift_core)

add_executable(vshift_ps3_runtime_probe
    tools/ps3_runtime_probe.cpp
)
target_link_libraries(vshift_ps3_runtime_probe PRIVATE vshift_core)

if(VSHIFT_BUILD_IOS)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
        message(FATAL_ERROR "VSHIFT_BUILD_IOS requires an iOS CMake toolchain")
    endif()
    enable_language(OBJCXX)

    add_executable(vshift_ios_jit_probe MACOSX_BUNDLE
        app/ios/main.mm
    )
    find_library(UIKIT_FRAMEWORK UIKit REQUIRED)
    find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
    find_library(CORE_GRAPHICS_FRAMEWORK CoreGraphics REQUIRED)
    find_library(QUARTZ_CORE_FRAMEWORK QuartzCore REQUIRED)
    find_library(UNIFORM_TYPE_IDENTIFIERS_FRAMEWORK UniformTypeIdentifiers REQUIRED)
    target_link_libraries(vshift_ios_jit_probe PRIVATE
        vshift_ps3_core
        ${UIKIT_FRAMEWORK}
        ${FOUNDATION_FRAMEWORK}
        ${CORE_GRAPHICS_FRAMEWORK}
        ${QUARTZ_CORE_FRAMEWORK}
        ${UNIFORM_TYPE_IDENTIFIERS_FRAMEWORK}
    )
    if(VSHIFT_BUILD_RPCS3_CORE)
        target_link_libraries(vshift_ios_jit_probe PRIVATE vshift_rpcs3_core)
        target_compile_definitions(vshift_ios_jit_probe PRIVATE VSHIFT_HAVE_RPCS3_CORE=1)
    endif()
    set_target_properties(vshift_ios_jit_probe PROPERTIES
        BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER org.vshift.VSHiftJITProbe
        MACOSX_BUNDLE_BUNDLE_NAME VSHiftJITProbe
        XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS
            "${CMAKE_CURRENT_SOURCE_DIR}/app/ios/VSHiftJITProbe.entitlements"
        MACOSX_BUNDLE_INFO_PLIST
            "${CMAKE_CURRENT_SOURCE_DIR}/app/ios/Info.plist"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        XCODE_ATTRIBUTE_UIRequiresFullScreen "YES"
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "15.0"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "org.vshift.VSHiftJITProbe"
    )
endif()
