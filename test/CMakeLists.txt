# One executable per test source; main() comes from test/doctest_main.cpp.
helios_add_test(reflection_test)
helios_apply_reflection(reflection_test)
helios_add_test(execution_test)
helios_add_test(abi_object_test)
helios_add_test(app_test)
helios_add_test(ui_scheduler_test)
helios_add_test(window_test)
helios_add_test(event_test)

# More options:
# helios_add_test(utils_test
#         SOURCES utils_test.cpp utils_helpers.cpp
#         LIBRARIES another_target
#         ARGS --verbose
#         TIMEOUT 60)

# Auto-register every test/**/*.cpp as its own executable:
# helios_discover_tests()
