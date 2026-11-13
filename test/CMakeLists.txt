# One executable per test source; main() comes from test/doctest_main.cpp.
helios_add_test(reflection_test)
helios_apply_reflection(reflection_test)
helios_add_test(execution_test)
helios_add_test(app_test)
helios_add_test(ui_scheduler_test)
helios_add_test(window_test)
helios_add_test(event_test)
# json_test and error_test parse JSON back with glaze; log_test does not need it.
helios_add_test(log_test)
helios_add_test(json_test LIBRARIES helios_json)
helios_add_test(error_test LIBRARIES helios_json)

# More options:
# helios_add_test(utils_test
#         SOURCES utils_test.cpp utils_helpers.cpp
#         LIBRARIES another_target
#         ARGS --verbose
#         TIMEOUT 60)

# Auto-register every test/**/*.cpp as its own executable:
# helios_discover_tests()
