# doctest comes from a git submodule and is used as source (header-only):
#   git submodule add https://github.com/doctest/doctest.git thirdparty/doctest
# Tests are skipped with a warning until the submodule is present.
include_guard(GLOBAL)

option(HELIOS_BUILD_TESTS "Build the test/ targets" ON)

set(HELIOS_DOCTEST_ROOT "${PROJECT_SOURCE_DIR}/thirdparty/doctest"
        CACHE PATH "doctest submodule root (contains doctest/doctest.h)")

set(HELIOS_TEST_MAIN_SOURCE "${PROJECT_SOURCE_DIR}/test/doctest_main.cpp"
        CACHE FILEPATH "Translation unit providing main() for the tests")

function(_helios_setup_doctest)
    if(TARGET helios_doctest)
        return()
    endif()

    if(EXISTS "${HELIOS_DOCTEST_ROOT}/doctest/doctest.h")
        add_library(helios_doctest INTERFACE)
        target_include_directories(helios_doctest SYSTEM INTERFACE "${HELIOS_DOCTEST_ROOT}")
        if(NOT TARGET doctest::doctest)
            add_library(doctest::doctest ALIAS helios_doctest)
        endif()
    endif()
endfunction()

function(helios_enable_tests)
    if(NOT HELIOS_BUILD_TESTS)
        return()
    endif()

    _helios_setup_doctest()

    if(NOT TARGET helios_doctest)
        message(WARNING
                "${PROJECT_NAME}: doctest not found at ${HELIOS_DOCTEST_ROOT}; tests skipped.\n"
                "  git submodule add https://github.com/doctest/doctest.git thirdparty/doctest\n"
                "  git submodule update --init thirdparty/doctest")
        return()
    endif()

    add_subdirectory("${PROJECT_SOURCE_DIR}/test" "${CMAKE_CURRENT_BINARY_DIR}/test")
endfunction()

# helios_add_test(<name> [SOURCES ...] [LIBRARIES ...] [ARGS ...]
#                 [WORKING_DIRECTORY <dir>] [TIMEOUT <sec>] [NO_MAIN])
function(helios_add_test name)
    if(NOT TARGET helios_doctest)
        message(FATAL_ERROR
                "${PROJECT_NAME}: helios_add_test(${name}) requires doctest; "
                "run: git submodule update --init thirdparty/doctest")
    endif()

    cmake_parse_arguments(T "NO_MAIN" "WORKING_DIRECTORY;TIMEOUT" "SOURCES;LIBRARIES;ARGS" ${ARGN})

    if(T_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
                "${PROJECT_NAME}: helios_add_test(${name}) got unknown arguments: ${T_UNPARSED_ARGUMENTS}")
    endif()

    if(NOT T_SOURCES)
        set(T_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${name}.cpp")
        if(NOT EXISTS "${T_SOURCES}")
            message(FATAL_ERROR
                    "${PROJECT_NAME}: helios_add_test(${name}): ${T_SOURCES} not found; "
                    "use SOURCES to list the sources.")
        endif()
    endif()

    if(NOT T_NO_MAIN)
        if(NOT EXISTS "${HELIOS_TEST_MAIN_SOURCE}")
            message(FATAL_ERROR "${PROJECT_NAME}: missing test main: ${HELIOS_TEST_MAIN_SOURCE}")
        endif()
        list(APPEND T_SOURCES "${HELIOS_TEST_MAIN_SOURCE}")
    endif()

    add_executable(${name} ${T_SOURCES})
    target_link_libraries(${name} PRIVATE ${PROJECT_NAME} helios_doctest ${T_LIBRARIES})
    helios_set_warnings(${name})
    helios_copy_mingw_runtime(${name})

    add_test(NAME ${name} COMMAND ${name} ${T_ARGS})

    if(T_WORKING_DIRECTORY)
        set_tests_properties(${name} PROPERTIES WORKING_DIRECTORY "${T_WORKING_DIRECTORY}")
    endif()
    if(T_TIMEOUT)
        set_tests_properties(${name} PROPERTIES TIMEOUT "${T_TIMEOUT}")
    endif()
endfunction()

# Register every test/**/*.cpp as its own executable (skips doctest_main.cpp and _*.cpp).
function(helios_discover_tests)
    cmake_parse_arguments(D "" "DIRECTORY" "" ${ARGN})
    if(NOT D_DIRECTORY)
        set(D_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()

    get_filename_component(_main_name "${HELIOS_TEST_MAIN_SOURCE}" NAME)

    file(GLOB_RECURSE _test_sources CONFIGURE_DEPENDS "${D_DIRECTORY}/*.cpp")
    foreach(_src IN LISTS _test_sources)
        get_filename_component(_filename "${_src}" NAME)
        if(_filename STREQUAL _main_name)
            continue()
        endif()

        get_filename_component(_stem "${_src}" NAME_WE)
        if(_stem MATCHES "^_")
            continue()
        endif()

        file(RELATIVE_PATH _rel "${D_DIRECTORY}" "${_src}")
        string(REGEX REPLACE "\\.cpp$" "" _name "${_rel}")
        string(REPLACE "/" "_" _name "${_name}")
        string(REPLACE "\\" "_" _name "${_name}")

        helios_add_test(${_name} SOURCES "${_src}")
    endforeach()
endfunction()
