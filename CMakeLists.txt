cmake_minimum_required(VERSION 3.21)

project(QuickkyExplorerTabs LANGUAGES CXX)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "Quickky Explorer Tabs requires Windows and MSVC.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Quickky Explorer Tabs requires an x86_64 target. Configure with -A x64.")
endif()

add_executable(QuickkyExplorerTabs WIN32 src/app/main.cpp)

set_target_properties(QuickkyExplorerTabs PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

target_compile_definitions(QuickkyExplorerTabs PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
)

target_compile_options(QuickkyExplorerTabs PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
target_link_libraries(QuickkyExplorerTabs PRIVATE user32 shell32)

include(CTest)

if(BUILD_TESTING)
    add_test(NAME app.smoke COMMAND QuickkyExplorerTabs --smoke-test)
    set_tests_properties(app.smoke PROPERTIES TIMEOUT 10)
endif()
