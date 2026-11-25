# Unit tests for xSchedule (Catch2).
#
# Modelled on xlights/tests/unit. Standalone project, separate from the
# Code::Blocks / Visual Studio builds:
#
#   cmake -S tests/unit -B build-tests -G Ninja
#   cmake --build build-tests && ctest --test-dir build-tests --output-on-failure
#
# xSchedule has no wx-free core, so unlike xLights' tests these link wxWidgets
# (found the usual way: wx-config on Linux, WXWIN / wxWidgets_ROOT_DIR on Windows).
# Tests only exercise logic, never construct windows, and run headless.
#
# Layout: the xSchedule / xlights sources under test go into the xschedule_testlib
# static library via sources.cmake; each tests/unit/test_*.cpp is globbed into the
# one test executable. Growing coverage = add a source to sources.cmake (once a test
# needs it) and drop a test_*.cpp next to this file.
cmake_minimum_required(VERSION 3.20)
project(xSchedule-unit-tests CXX C)

set(XS_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE INTERNAL "")
get_filename_component(XS_REPO_ROOT "${XS_REPO_ROOT}" ABSOLUTE)
set(XS_SRC "${XS_REPO_ROOT}/xSchedule")
set(XL_ROOT "${XS_REPO_ROOT}/xlights")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

include(CTest)

find_package(Catch2 3 QUIET)
if(NOT Catch2_FOUND)
    include(FetchContent)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.8.1)
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
endif()

# Windows: use the same prebuilt xLights dependency bundle as the application
# (xlights/ci_scripts/fetch_dependencies.ps1 -Only Dependencies stages it), unless a
# local wxWidgets is given with WXWIDGETS_ROOT / wxWidgets_ROOT_DIR.
set(XL_BUNDLE "${XL_ROOT}/dependencies-bundle")
if(WIN32)
    if(NOT wxWidgets_ROOT_DIR)
        if(DEFINED ENV{WXWIDGETS_ROOT})
            set(wxWidgets_ROOT_DIR "$ENV{WXWIDGETS_ROOT}" CACHE PATH "wxWidgets root directory")
        elseif(EXISTS "${XL_BUNDLE}/wxWidgets/include/wx/wx.h")
            set(wxWidgets_ROOT_DIR "${XL_BUNDLE}/wxWidgets" CACHE PATH "wxWidgets root directory")
        else()
            message(FATAL_ERROR "wxWidgets not found: run xlights/ci_scripts/fetch_dependencies.ps1 -Only Dependencies, or set WXWIDGETS_ROOT")
        endif()
    endif()
    if(NOT wxWidgets_LIB_DIR)
        set(wxWidgets_LIB_DIR "${wxWidgets_ROOT_DIR}/lib/vc_x64_lib" CACHE PATH "wxWidgets library directory")
    endif()
    if(NOT CURL_INCLUDE_DIR AND EXISTS "${XL_BUNDLE}/include/curl/curl.h")
        set(CURL_INCLUDE_DIR "${XL_BUNDLE}/include")
        set(CURL_LIBRARY "${XL_BUNDLE}/lib/libcurl.lib")
    endif()
endif()

find_package(wxWidgets 3.3 REQUIRED COMPONENTS core base xml net)
include(${wxWidgets_USE_FILE})
if(WIN32)
    link_directories(${wxWidgets_LIB_DIR})  # the wx static libs pull in their own deps (wxwebp, ...) by name
endif()

include(${CMAKE_CURRENT_LIST_DIR}/sources.cmake)
add_library(xschedule_testlib STATIC ${XS_TEST_SOURCES})
target_include_directories(xschedule_testlib PUBLIC
    ${XS_SRC}
    ${XL_ROOT}/include
    ${XL_ROOT}/src-core
    ${XL_ROOT}/src-core/utils
    ${XL_ROOT}/src-ui-wx
    ${XL_ROOT}/dependencies
    ${XS_REPO_ROOT}/dependencies
    ${XS_REPO_ROOT}/dependencies/pugixml/src
    ${XS_REPO_ROOT}/dependencies/spdlog/include
    ${CMAKE_CURRENT_LIST_DIR}                 # support/
    ${XL_ROOT}/tests/unit)                    # xLights' support/TempDir.h
# Same defines as the application build (xSchedule.cbp / xSchedule.vcxproj).
target_compile_definitions(xschedule_testlib PUBLIC wxUSE_UNICODE EXCLUDENETWORKUI DISCOVERYONLY)
if(WIN32)
    target_compile_definitions(xschedule_testlib PUBLIC __MSVC__ NOMINMAX _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_definitions(xschedule_testlib PUBLIC LINUX)
endif()
if(MSVC)
    target_compile_options(xschedule_testlib PUBLIC /utf-8 /bigobj)
endif()
find_package(CURL REQUIRED)
target_link_libraries(xschedule_testlib PUBLIC ${wxWidgets_LIBRARIES} CURL::libcurl)
if(WIN32)
    target_link_libraries(xschedule_testlib PUBLIC iphlpapi ws2_32)  # xScheduleApp.cpp pulls these in with #pragma
elseif(NOT APPLE)
    find_package(X11 REQUIRED)  # JobPool calls XInitThreads
    target_link_libraries(xschedule_testlib PUBLIC X11::X11)
endif()

file(GLOB XS_TEST_SOURCES_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/test_*.cpp)
add_executable(xschedule_unit_tests ${XS_TEST_SOURCES_TESTS} ${CMAKE_CURRENT_LIST_DIR}/support/main.cpp)
target_link_libraries(xschedule_unit_tests PRIVATE xschedule_testlib Catch2::Catch2)
if(WIN32)
    # The bundle's DLLs next to the exe; catch_discover_tests runs it as part of the build.
    add_custom_command(TARGET xschedule_unit_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${XL_ROOT}/bin64/libcurl.dll" $<TARGET_FILE_DIR:xschedule_unit_tests>)
endif()

include(Catch)
catch_discover_tests(xschedule_unit_tests)
