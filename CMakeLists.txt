cmake_minimum_required(VERSION 3.21)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" QUICKKY_VERSION)
string(STRIP "${QUICKKY_VERSION}" QUICKKY_VERSION)
if(NOT QUICKKY_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "VERSION must contain MAJOR.MINOR.PATCH.")
endif()
project(QuickkyExplorerTabs VERSION "${QUICKKY_VERSION}" LANGUAGES CXX RC)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/VERSION"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/locales/en-US.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/locales/zh-CN.json"
)
foreach(locale IN ITEMS en-US zh-CN)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/resources/locales/${locale}.json" catalog)
    string(SUBSTRING "${locale}" 0 2 prefix)
    foreach(section IN ITEMS tray about)
        if(section STREQUAL "tray")
            set(keys startup enableTabs about exit)
        else()
            set(keys title author version buildTime)
        endif()
        foreach(key IN LISTS keys)
            string(JSON text GET "${catalog}" "${section}" "${key}")
            string(REPLACE "\\" "\\\\" text "${text}")
            string(REPLACE "\"" "\\\"" text "${text}")
            string(REPLACE "\n" "\\n" text "${text}")
            set("${prefix}_${key}" "${text}")
        endforeach()
    endforeach()
endforeach()
configure_file(resources/locales/tray_strings.h.in generated/tray_strings.h @ONLY)
configure_file(resources/app.rc.in generated/app.rc @ONLY)
add_custom_target(QuickkyBuildMetadata ALL
    COMMAND "${CMAKE_COMMAND}" "-DVERSION_FILE=${CMAKE_CURRENT_SOURCE_DIR}/VERSION"
        "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/generated"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/build_metadata.cmake"
    BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/generated/build_metadata.h"
    VERBATIM
)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "Quickky Explorer Tabs requires Windows and MSVC.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Quickky Explorer Tabs requires an x86_64 target. Configure with -A x64.")
endif()

function(quickky_configure_target target)
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
    )
    target_compile_definitions(${target} PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
        WINVER=0x0A00 _WIN32_WINNT=0x0A00
    )
    target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
endfunction()

# Package only the redistributables selected by the active MSVC toolchain.
set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
include(InstallRequiredSystemLibraries)
set(QUICKKY_RUNTIME_NAMES "")
set(QUICKKY_RUNTIME_PATHS "")
foreach(runtime IN LISTS CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
    get_filename_component(name "${runtime}" NAME)
    string(APPEND QUICKKY_RUNTIME_NAMES "    L\"${name}\",\n")
    string(APPEND QUICKKY_RUNTIME_PATHS "${runtime}\n")
endforeach()
configure_file(resources/runtime_files.h.in generated/runtime_files.h @ONLY)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/runtime-files.txt" "${QUICKKY_RUNTIME_PATHS}")

add_library(quickky_core STATIC
    src/group/group_model.cpp
    src/capture/capture_coordinator.cpp
)
target_include_directories(quickky_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
quickky_configure_target(quickky_core)

add_library(quickky_capture STATIC
    src/capture/explorer_window_probe.cpp
    src/capture/explorer_discovery.cpp
)
target_link_libraries(quickky_capture PUBLIC quickky_core ole32 uuid dwmapi advapi32 user32)
quickky_configure_target(quickky_capture)

add_library(quickky_explorer STATIC
    src/explorer/native_tab_guard.cpp
    src/explorer/folder_location.cpp
    src/explorer/native_frame.cpp
)
target_include_directories(quickky_explorer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(quickky_explorer PUBLIC user32 gdi32 comctl32 ole32 oleaut32 shell32 uuid)
quickky_configure_target(quickky_explorer)

add_library(quickky_chrome STATIC src/chrome/tab_strip.cpp)
target_include_directories(quickky_chrome PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(quickky_chrome PUBLIC d2d1 dwrite dwmapi advapi32 user32)
quickky_configure_target(quickky_chrome)

add_library(QuickkyExplorerHook SHARED src/hook/explorer_hook.cpp)
target_link_libraries(QuickkyExplorerHook PRIVATE quickky_explorer quickky_chrome comctl32 ole32 uuid)
quickky_configure_target(QuickkyExplorerHook)

add_library(quickky_hook STATIC src/hook/hook_client.cpp)

target_include_directories(quickky_hook PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(quickky_hook PUBLIC quickky_core quickky_explorer ole32 user32)
quickky_configure_target(quickky_hook)

add_library(quickky_workspace STATIC src/group/workspace_controller.cpp)
target_link_libraries(quickky_workspace PUBLIC quickky_capture quickky_explorer quickky_hook)
quickky_configure_target(quickky_workspace)

add_library(quickky_settings STATIC src/settings/preferences.cpp)
target_include_directories(quickky_settings PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(quickky_settings PUBLIC advapi32)
quickky_configure_target(quickky_settings)

add_executable(QuickkyExplorerTabs WIN32 src/app/main.cpp src/app/tray_icon.cpp
    resources/app.manifest "${CMAKE_CURRENT_BINARY_DIR}/generated/app.rc")
set_target_properties(QuickkyExplorerTabs PROPERTIES OUTPUT_NAME QTabs)
target_include_directories(QuickkyExplorerTabs PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources")
target_link_libraries(QuickkyExplorerTabs PRIVATE quickky_workspace quickky_settings shell32 comctl32)
add_dependencies(QuickkyExplorerTabs QuickkyExplorerHook QuickkyBuildMetadata)
quickky_configure_target(QuickkyExplorerTabs)

include(CTest)

if(BUILD_TESTING)
    add_executable(QuickkyNativeTransferProbe tests/native_transfer_probe.cpp)
    target_link_libraries(QuickkyNativeTransferProbe PRIVATE quickky_capture quickky_hook)
    add_dependencies(QuickkyNativeTransferProbe QuickkyExplorerHook)
    quickky_configure_target(QuickkyNativeTransferProbe)
    add_executable(QuickkyWorkspaceBridgeTests tests/workspace_bridge_tests.cpp)
    target_link_libraries(QuickkyWorkspaceBridgeTests PRIVATE quickky_hook quickky_explorer quickky_settings dwmapi)
    add_dependencies(QuickkyWorkspaceBridgeTests QuickkyExplorerHook)
    quickky_configure_target(QuickkyWorkspaceBridgeTests)
    add_test(NAME workspace.bridge COMMAND QuickkyWorkspaceBridgeTests)
    set_tests_properties(workspace.bridge PROPERTIES TIMEOUT 15)
    add_executable(QuickkyNativeGuardTests tests/native_guard_tests.cpp)
    target_link_libraries(QuickkyNativeGuardTests PRIVATE quickky_explorer)
    quickky_configure_target(QuickkyNativeGuardTests)
    add_test(NAME explorer.native_guard COMMAND QuickkyNativeGuardTests)
    add_executable(QuickkyNativeGuardProbe tests/native_guard_probe.cpp)
    target_link_libraries(QuickkyNativeGuardProbe PRIVATE quickky_capture quickky_hook)
    add_dependencies(QuickkyNativeGuardProbe QuickkyExplorerHook)
    quickky_configure_target(QuickkyNativeGuardProbe)
    add_executable(QuickkyCoreTests tests/core_tests.cpp)
    target_link_libraries(QuickkyCoreTests PRIVATE quickky_core)
    quickky_configure_target(QuickkyCoreTests)
    add_test(NAME core.model COMMAND QuickkyCoreTests)
    add_executable(QuickkyDiscoveryTests tests/discovery_tests.cpp)
    target_link_libraries(QuickkyDiscoveryTests PRIVATE quickky_capture)
    quickky_configure_target(QuickkyDiscoveryTests)
    add_test(NAME capture.discovery COMMAND QuickkyDiscoveryTests)
    add_test(NAME app.smoke COMMAND QuickkyExplorerTabs --smoke-test)
    set_tests_properties(core.model capture.discovery app.smoke explorer.native_guard PROPERTIES TIMEOUT 10)
endif()
