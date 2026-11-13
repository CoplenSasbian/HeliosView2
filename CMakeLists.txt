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
include(HeliosTests)

add_library(${PROJECT_NAME} SHARED
        src/${PROJECT_NAME}.cpp
        src/abi/app_factory.cpp)

if(WIN32)
    target_sources(${PROJECT_NAME} PRIVATE
            src/win32/app_win32.cpp
            src/win32/window_win32.cpp)
    target_link_libraries(${PROJECT_NAME} PRIVATE user32 ole32)
else()
    target_sources(${PROJECT_NAME} PRIVATE src/portable/app_stub.cpp)
endif()

add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

helios_apply_cxx26(${PROJECT_NAME})

# Public: Execution.h is part of the installed interface.
target_link_libraries(${PROJECT_NAME} PUBLIC helios_exec)

target_compile_definitions(${PROJECT_NAME} PRIVATE
        HELIOSVIEW2_VERSION="${PROJECT_VERSION}")

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
