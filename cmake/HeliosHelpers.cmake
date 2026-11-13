include_guard(GLOBAL)

function(helios_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
endfunction()

option(HELIOS_COPY_MINGW_RUNTIME "Copy MinGW runtime DLLs next to the binaries" ON)

# Copy libstdc++/libgcc/libwinpthread so tests run from an IDE without PATH setup.
function(helios_copy_mingw_runtime target)
    if(NOT MINGW OR NOT HELIOS_COPY_MINGW_RUNTIME)
        return()
    endif()

    get_filename_component(_compiler_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
    foreach(_dll IN ITEMS libstdc++-6.dll libgcc_s_seh-1.dll libgcc_s_dw2-1.dll libwinpthread-1.dll)
        if(EXISTS "${_compiler_bin}/${_dll}")
            add_custom_command(TARGET ${target} POST_BUILD
                    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                            "${_compiler_bin}/${_dll}" "$<TARGET_FILE_DIR:${target}>"
                    VERBATIM)
        endif()
    endforeach()
endfunction()
