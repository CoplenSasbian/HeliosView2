# Execution backend: std::execution (C++26, P2300) when the standard library
# provides it, otherwise NVIDIA stdexec. One INTERFACE target hides the choice.
include_guard(GLOBAL)

set(HELIOS_EXECUTION_IMPL "auto" CACHE STRING
        "Execution backend: auto (std when available, else stdexec), std, or stdexec")
set_property(CACHE HELIOS_EXECUTION_IMPL PROPERTY STRINGS auto std stdexec)

set(HELIOS_STDEXEC_ROOT "${PROJECT_SOURCE_DIR}/thirdparty/stdexec" CACHE PATH
        "stdexec checkout (contains include/stdexec/execution.hpp)")

if(NOT HELIOS_EXECUTION_IMPL MATCHES "^(auto|std|stdexec)$")
    message(FATAL_ERROR
            "HELIOS_EXECUTION_IMPL='${HELIOS_EXECUTION_IMPL}' is invalid; use auto, std, or stdexec.")
endif()

function(_helios_detect_std_senders out)
    include(CheckCXXSourceCompiles)
    check_cxx_source_compiles("
        #include <version>
        #if !defined(__cpp_lib_senders) || __cpp_lib_senders < 202406L
        #  error no C++26 senders
        #endif
        int main() { return 0; }" HELIOS_STD_SENDERS_AVAILABLE)
    set(${out} "${HELIOS_STD_SENDERS_AVAILABLE}" PARENT_SCOPE)
endfunction()

set(_helios_use_std_execution FALSE)

if(HELIOS_EXECUTION_IMPL STREQUAL "std")
    _helios_detect_std_senders(_helios_std_available)
    if(NOT _helios_std_available)
        message(FATAL_ERROR
                "HELIOS_EXECUTION_IMPL=std requested, but this standard library has no "
                "C++26 senders (__cpp_lib_senders). Use the stdexec backend instead.")
    endif()
    set(_helios_use_std_execution TRUE)
elseif(HELIOS_EXECUTION_IMPL STREQUAL "auto")
    _helios_detect_std_senders(_helios_std_available)
    if(_helios_std_available)
        set(_helios_use_std_execution TRUE)
    endif()
endif()

if(NOT _helios_use_std_execution)
    if(NOT EXISTS "${HELIOS_STDEXEC_ROOT}/include/stdexec/execution.hpp")
        message(FATAL_ERROR
                "stdexec not found at ${HELIOS_STDEXEC_ROOT}.\n"
                "  git submodule add https://github.com/NVIDIA/stdexec.git thirdparty/stdexec\n"
                "  git -C thirdparty/stdexec checkout 7df6de8eeec667c8adce66fa5a3e17cfd6849e48\n"
                "Or build with a standard library that provides __cpp_lib_senders.")
    endif()
endif()

add_library(helios_exec INTERFACE)
target_compile_features(helios_exec INTERFACE cxx_std_26)

if(_helios_use_std_execution)
    message(STATUS "HeliosView2: execution backend = std::execution")
    target_compile_definitions(helios_exec INTERFACE HELIOS_USE_STD_EXECUTION=1)
    set(HELIOS_EXECUTION_BACKEND "std" CACHE INTERNAL "Selected execution backend")
else()
    message(STATUS "HeliosView2: execution backend = stdexec (${HELIOS_STDEXEC_ROOT})")
    target_include_directories(helios_exec SYSTEM INTERFACE "${HELIOS_STDEXEC_ROOT}/include")
    target_compile_definitions(helios_exec INTERFACE HELIOS_USE_STD_EXECUTION=0)
    set(HELIOS_EXECUTION_BACKEND "stdexec" CACHE INTERNAL "Selected execution backend")
endif()
