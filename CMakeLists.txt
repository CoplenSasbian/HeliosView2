cmake_minimum_required(VERSION 3.30)

# Library name == directory name.
get_filename_component(HELIOS_PROJECT_NAME "${CMAKE_CURRENT_SOURCE_DIR}" NAME)

project(${HELIOS_PROJECT_NAME}
        VERSION 0.1.0
        DESCRIPTION "C++26 reflection based dynamic library"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# This project uses headers and P2996 reflection, not C++20 modules. Leaving
# CMake's module scanner enabled makes Ninja/CLion pass -fmodules-ts to every
# TU; GCC 16 ICEs in stdexec under that mode (forwarding_query/__query.hpp).
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

include(GNUInstallDirs)

# Keep the DLL next to the executables so tests run without PATH setup.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(HeliosReflection)
include(HeliosHelpers)
include(HeliosExecution)
include(HeliosJson)
include(HeliosTests)

# A shared library is a module boundary, not an ABI contract; a static one
# removes it entirely. The public headers are the same either way, so an
# application can start static and switch later.
option(HELIOS_BUILD_SHARED "Build ${PROJECT_NAME} as a shared library" ON)
if(HELIOS_BUILD_SHARED)
    set(HELIOS_LIBRARY_TYPE SHARED)
else()
    set(HELIOS_LIBRARY_TYPE STATIC)
endif()

# No frozen ABI: the exported API is ordinary C++ (std::string, helios::error,
# classes with virtual functions such as base_window), so the headers and the
# DLL are always built together with the same compiler, standard library and
# flags. The delivered artifact is one binary per standard library version, not
# one binary that every toolchain can load. That is what lets app be constructed
# directly and lets base_window be derived by the application.
# HELIOS_BUILD_SHARED=OFF removes the module boundary entirely.
add_library(${PROJECT_NAME} ${HELIOS_LIBRARY_TYPE}
        src/${PROJECT_NAME}.cpp
        src/App.cpp
        src/Window.cpp
        src/WindowImpl.cpp
        src/Error.cpp
        src/Log.cpp)

if(NOT HELIOS_BUILD_SHARED)
    # Turns HELIOSVIEW2_EXPORT into nothing for every consumer of the headers.
    target_compile_definitions(${PROJECT_NAME} PUBLIC HELIOSVIEW2_STATIC_BUILD)
endif()

# Windowing backend. The portable headers (App.h, Window.h, Event.h,
# HandlerScanner.h) never mention a platform; everything OS specific lives in
# one src/<backend>/ directory and one optional platform header. To add a
# platform: create src/<backend>/, implement app and base_window there, then
# extend the branches below.
set(HELIOS_WINDOW_BACKEND "auto" CACHE STRING
        "Windowing backend: auto (win32 on Windows, stub elsewhere), win32, stub")
set_property(CACHE HELIOS_WINDOW_BACKEND PROPERTY STRINGS auto win32 stub)

set(_helios_backend "${HELIOS_WINDOW_BACKEND}")
if(_helios_backend STREQUAL "auto")
    if(WIN32)
        set(_helios_backend win32)
    else()
        set(_helios_backend stub)
    endif()
endif()

if(_helios_backend STREQUAL "win32")
    target_sources(${PROJECT_NAME} PRIVATE
            src/win32/app_win32.cpp
            src/win32/window_win32.cpp)
    target_link_libraries(${PROJECT_NAME} PRIVATE user32 ole32)
elseif(_helios_backend STREQUAL "stub")
    target_sources(${PROJECT_NAME} PRIVATE
            src/portable/app_stub.cpp
            src/portable/window_stub.cpp)
else()
    message(FATAL_ERROR
            "${PROJECT_NAME}: unknown HELIOS_WINDOW_BACKEND '${HELIOS_WINDOW_BACKEND}'.\n"
            "Use auto, win32 or stub, or add a branch for the new backend.")
endif()

message(STATUS "${PROJECT_NAME}: windowing backend '${_helios_backend}'")

add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

helios_apply_cxx26(${PROJECT_NAME})

# Public: Execution.h is part of the installed interface.
target_link_libraries(${PROJECT_NAME} PUBLIC helios_exec)

# JSON (glaze) is a build-time dependency: no public header exposes it, so other
# targets link helios_json themselves when they need it.
helios_link_json(${PROJECT_NAME})

target_compile_definitions(${PROJECT_NAME} PRIVATE
        HELIOSVIEW2_VERSION="${PROJECT_VERSION}")

# Log patterns print a source file relative to the project root, so the root is
# baked into the build interface. Consumers of an installed package fall back to
# the absolute path recorded by the compiler.
target_compile_definitions(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:HELIOS_PROJECT_ROOT="${CMAKE_CURRENT_SOURCE_DIR}">)

set(HELIOS_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
include(GenerateExportHeader)
generate_export_header(${PROJECT_NAME}
        BASE_NAME HELIOSVIEW2
        EXPORT_FILE_NAME "${HELIOS_GENERATED_DIR}/${PROJECT_NAME}/${PROJECT_NAME}_export.h"
        STATIC_DEFINE HELIOSVIEW2_STATIC_BUILD)

target_include_directories(${PROJECT_NAME}
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${HELIOS_GENERATED_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

set_target_properties(${PROJECT_NAME} PROPERTIES
        OUTPUT_NAME "${PROJECT_NAME}"
        VERSION "${PROJECT_VERSION}"
        SOVERSION "${PROJECT_VERSION_MAJOR}")

if(WIN32)
    # HeliosView2.dll instead of libHeliosView2.dll.
    set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
endif()

helios_set_warnings(${PROJECT_NAME})
helios_copy_mingw_runtime(${PROJECT_NAME})

if(HELIOS_BUILD_TESTS)
    # Must be called in the top-level directory for ctest to find the tests.
    enable_testing()
endif()
helios_enable_tests()

option(HELIOS_BUILD_EXAMPLES "Build example programs" ON)
if(HELIOS_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Execution.h includes <stdexec/execution.hpp> when the standard library has no
# senders yet, so the installed headers need the pinned stdexec headers too.
if(HELIOS_EXECUTION_BACKEND STREQUAL "stdexec")
    install(DIRECTORY "${HELIOS_STDEXEC_ROOT}/include/" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()

install(FILES "${HELIOS_GENERATED_DIR}/${PROJECT_NAME}/${PROJECT_NAME}_export.h"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})
