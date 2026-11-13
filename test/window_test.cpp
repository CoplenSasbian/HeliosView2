#include <HeliosView2/App.h>
#include <HeliosView2/Window.h>

#include <doctest/doctest.h>

#include <cstring>

namespace abi = helios::abi;

#if defined(_WIN32)
TEST_CASE("create_window creates a native window") {
    helios::app_options app_options{};
    abi::ptr<helios::app> application;

    REQUIRE(abi::succeeded(helios::create_app(&app_options, std::out_ptr(application))));
    REQUIRE(application != nullptr);

    abi::window_options options{};
    options.title = "HeliosView2";
    options.title_size = std::strlen(options.title);
    options.width = 320;
    options.height = 240;
    options.visible = 0;
    options.resizable = 1;

    abi::ptr<helios::base_window> window;
    REQUIRE(abi::succeeded(application->create_window(&options, std::out_ptr(window))));
    REQUIRE(window != nullptr);

    CHECK(application->window_count() == 1);
    CHECK(window->native_handle() != nullptr);
    CHECK_FALSE(window->is_closed());

    CHECK(abi::succeeded(window->show()));
    CHECK(abi::succeeded(window->hide()));
    CHECK(abi::succeeded(window->set_title("Renamed", 7)));

    CHECK(abi::succeeded(window->close()));
    CHECK(window->is_closed());
    CHECK(window->native_handle() == nullptr);
    CHECK(application->window_count() == 0);

    // Closing the last window requests app shutdown (default option).
    std::int32_t exit_code = -1;
    CHECK(abi::succeeded(application->run(&exit_code)));
    CHECK(exit_code == 0);
}

TEST_CASE("releasing app destroys open windows") {
    helios::app_options app_options{};
    abi::ptr<helios::app> application;

    REQUIRE(abi::succeeded(helios::create_app(&app_options, std::out_ptr(application))));
    REQUIRE(application != nullptr);

    abi::window_options options{};
    options.title = "Lifetime";
    options.title_size = std::strlen(options.title);

    abi::ptr<helios::base_window> window;
    REQUIRE(abi::succeeded(application->create_window(&options, std::out_ptr(window))));
    REQUIRE(window != nullptr);

    application.reset();
    CHECK(window->is_closed());
    CHECK(window->native_handle() == nullptr);

    window.reset();
}
#endif
