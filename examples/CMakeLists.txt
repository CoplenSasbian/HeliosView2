if(WIN32)
    add_executable(window_example
            main.cpp
            example_window.cpp
            example_window.h)
    target_link_libraries(window_example PRIVATE ${PROJECT_NAME})
    helios_apply_cxx26(window_example)
    # handler_scanner needs reflection in the example's own translation units.
    helios_apply_reflection(window_example)
    helios_set_warnings(window_example)
    helios_copy_mingw_runtime(window_example)

    # Show the example in the IDE run configuration list under its source name.
    set_target_properties(window_example PROPERTIES
            OUTPUT_NAME "window_example"
            FOLDER "examples")
else()
    message(STATUS "${PROJECT_NAME}: window_example needs the Win32 backend; skipped")
endif()
