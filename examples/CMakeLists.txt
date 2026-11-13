if(NOT WIN32)
    message(STATUS "${PROJECT_NAME}: window_example needs the Win32 backend; skipped")
    return()
endif()

add_executable(window_example window_example.cpp)
target_link_libraries(window_example PRIVATE ${PROJECT_NAME})
helios_apply_cxx26(window_example)
helios_set_warnings(window_example)
helios_copy_mingw_runtime(window_example)

# Show the example in the IDE run configuration list under its source name.
set_target_properties(window_example PROPERTIES
        OUTPUT_NAME "window_example"
        FOLDER "examples")
