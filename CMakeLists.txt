cmake_minimum_required(VERSION 3.24)

project(VSHift VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(VSHIFT_BUILD_TESTS "Build host-side tests" ON)
option(VSHIFT_BUILD_IOS "Build the iOS application target" OFF)

add_library(vshift_core STATIC
    core/cpu/executable_memory.cpp
    core/cpu/arm64_jit.cpp
    core/cpu/x86_decoder.cpp
    core/loader/elf.cpp
    core/firmware/catalog.cpp
    core/firmware/slb2.cpp
    core/firmware/pup.cpp
)

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

    add_test(NAME vshift_jit_smoke COMMAND vshift_jit_smoke)
    add_test(NAME vshift_slb2_test COMMAND vshift_slb2_test)
    add_test(NAME vshift_pup_test COMMAND vshift_pup_test)
    add_test(NAME vshift_catalog_test COMMAND vshift_catalog_test)
    add_test(NAME vshift_elf_test COMMAND vshift_elf_test)
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
    find_library(UNIFORM_TYPE_IDENTIFIERS_FRAMEWORK UniformTypeIdentifiers REQUIRED)
    target_link_libraries(vshift_ios_jit_probe PRIVATE
        vshift_core
        ${UIKIT_FRAMEWORK}
        ${FOUNDATION_FRAMEWORK}
        ${UNIFORM_TYPE_IDENTIFIERS_FRAMEWORK}
    )
    set_target_properties(vshift_ios_jit_probe PROPERTIES
        BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER org.vshift.VSHiftJITProbe
        MACOSX_BUNDLE_BUNDLE_NAME VSHiftJITProbe
        XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS
            "${CMAKE_CURRENT_SOURCE_DIR}/app/ios/VSHiftJITProbe.entitlements"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "15.0"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "org.vshift.VSHiftJITProbe"
    )
endif()
