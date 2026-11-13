# JSON support: glaze comes from a git submodule and is header-only (MIT):
#   git submodule add https://github.com/stephenberry/glaze.git thirdparty/glaze
#
# Everything links the INTERFACE target below instead of adding include paths by
# hand: it marks glaze as a SYSTEM include, so its warnings stay out of this
# project's -Wall -Wextra build, and swapping the JSON backend later means
# touching this file only.

include_guard(GLOBAL)

set(HELIOS_JSON_ROOT "${PROJECT_SOURCE_DIR}/thirdparty/glaze"
        CACHE PATH "glaze submodule root (contains include/glaze/glaze.hpp)")

function(helios_setup_json)
    if(TARGET helios_json)
        return()
    endif()

    if(NOT EXISTS "${HELIOS_JSON_ROOT}/include/glaze/glaze.hpp")
        message(FATAL_ERROR
                "${PROJECT_NAME}: glaze not found at ${HELIOS_JSON_ROOT}.\n"
                "  git submodule add https://github.com/stephenberry/glaze.git thirdparty/glaze\n"
                "  git submodule update --init thirdparty/glaze")
    endif()

    add_library(helios_json INTERFACE)
    target_compile_features(helios_json INTERFACE cxx_std_23)
    target_include_directories(helios_json SYSTEM INTERFACE
            "${HELIOS_JSON_ROOT}/include")

    if(NOT TARGET glaze::glaze)
        add_library(glaze::glaze ALIAS helios_json)
    endif()
endfunction()

# helios_link_json(<target>): adds glaze to a target without exposing it further.
function(helios_link_json target)
    helios_setup_json()
    target_link_libraries(${target} PRIVATE helios_json)
endfunction()
