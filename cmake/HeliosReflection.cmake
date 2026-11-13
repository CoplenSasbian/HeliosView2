# C++26 options and optional P2996 reflection support.
include_guard(GLOBAL)

set(HELIOS_REFLECTION_FLAGS "")

if(MSVC)
    # /std:c++latest (from cxx_std_26) is expected to provide reflection.
    add_compile_options(/utf-8)
else()
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-freflection" HELIOS_COMPILER_HAS_REFLECTION)
    if(HELIOS_COMPILER_HAS_REFLECTION)
        list(APPEND HELIOS_REFLECTION_FLAGS -freflection)
    endif()
endif()

# Every C++ target uses C++26. Reflection is deliberately NOT public: mixing
# -freflection with large template libraries (stdexec, etc.) can trigger
# compiler ICEs, and the ABI/execution layers do not need reflection.
function(helios_apply_cxx26 target)
    target_compile_features(${target} PUBLIC cxx_std_26)
endfunction()

# Opt-in P2996 reflection. Apply only to translation units that include <meta>
# and use std::meta. Do not make this PUBLIC unless every consumer needs it.
function(helios_apply_reflection target)
    if(NOT HELIOS_REFLECTION_FLAGS)
        message(FATAL_ERROR
                "${PROJECT_NAME}: reflection requested for target '${target}', but the "
                "selected compiler does not accept -freflection:\n"
                "  ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\n"
                "  ${CMAKE_CXX_COMPILER}\n"
                "Use GCC 16+ (P2996) or disable the reflection target.")
    endif()

    target_compile_options(${target} PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:${HELIOS_REFLECTION_FLAGS}>)
endfunction()
