cmake_minimum_required(VERSION 3.21)

project(QuickkyExplorerTabs LANGUAGES CXX)

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

add_executable(QuickkyExplorerTabs WIN32 src/app/main.cpp)
target_link_libraries(QuickkyExplorerTabs PRIVATE quickky_capture shell32)
quickky_configure_target(QuickkyExplorerTabs)

include(CTest)

if(BUILD_TESTING)
    add_executable(QuickkyCoreTests tests/core_tests.cpp)
    target_link_libraries(QuickkyCoreTests PRIVATE quickky_core)
    quickky_configure_target(QuickkyCoreTests)
    add_test(NAME core.model COMMAND QuickkyCoreTests)
    add_executable(QuickkyDiscoveryTests tests/discovery_tests.cpp)
    target_link_libraries(QuickkyDiscoveryTests PRIVATE quickky_capture)
    quickky_configure_target(QuickkyDiscoveryTests)
    add_test(NAME capture.discovery COMMAND QuickkyDiscoveryTests)
    add_test(NAME app.smoke COMMAND QuickkyExplorerTabs --smoke-test)
    set_tests_properties(core.model capture.discovery app.smoke PROPERTIES TIMEOUT 10)
endif()
