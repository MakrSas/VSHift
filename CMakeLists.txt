cmake_minimum_required(VERSION 3.24)

project(VSHift VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(VSHIFT_BUILD_TESTS "Build host-side tests" ON)
option(VSHIFT_BUILD_IOS "Build the iOS application target" OFF)
option(VSHIFT_BUILD_PS3 "Build the real RPCS3 PS3 core integration" ON)

add_library(vshift_core STATIC
    core/boot/ps4_boot.cpp
    core/boot/synthetic_boot.cpp
    core/cpu/executable_memory.cpp
    core/cpu/arm64_jit.cpp
    core/cpu/ir.cpp
    core/cpu/x86_runtime.cpp
    core/cpu/x86_decoder.cpp
    core/memory/guest_memory.cpp
    core/loader/elf.cpp
    core/loader/elf_dynamic.cpp
    core/loader/elf_loader.cpp
    core/loader/self.cpp
    core/loader/self_loader.cpp
    core/firmware/catalog.cpp
    core/firmware/vfs.cpp
    core/firmware/slb2.cpp
    core/firmware/pup.cpp
    core/hle/kernel.cpp
    core/video/framebuffer.cpp
    core/frontend/media_store.cpp
)

if(VSHIFT_BUILD_PS3)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/rpcs3_core.cmake)
endif()

target_include_directories(vshift_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(vshift_core PUBLIC cxx_std_20)

if(APPLE)
    target_compile_definitions(vshift_core PRIVATE VSHIFT_APPLE=1)
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
    find_library(UNIFORM_TYPE_IDENTIFIERS_FRAMEWORK UniformTypeIdentifiers REQUIRED)
    target_link_libraries(vshift_ios_jit_probe PRIVATE
        vshift_core
        ${UIKIT_FRAMEWORK}
        ${FOUNDATION_FRAMEWORK}
        ${CORE_GRAPHICS_FRAMEWORK}
        ${UNIFORM_TYPE_IDENTIFIERS_FRAMEWORK}
    )
    if(TARGET vshift_ps3_core)
        target_link_libraries(vshift_ios_jit_probe PRIVATE vshift_ps3_core)
    endif()
    set_target_properties(vshift_ios_jit_probe PROPERTIES
        BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER org.vshift.VSHiftJITProbe
        MACOSX_BUNDLE_BUNDLE_NAME VSHiftJITProbe
        XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS
            "${CMAKE_CURRENT_SOURCE_DIR}/app/ios/VSHiftJITProbe.entitlements"
        XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "15.0"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "org.vshift.VSHiftJITProbe"
    )
endif()
