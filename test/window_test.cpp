#include <HeliosView2/App.h>
#include <HeliosView2/Window.h>

#include "test_support.h"

#include <doctest/doctest.h>

#include <cstdint>
#include <memory>
#include <thread>
#include <utility>

#if defined(_WIN32)
TEST_CASE("create_window creates a native window") {
    std::unique_ptr<helios::app> application = test_support::app();

    std::shared_ptr<helios::base_window> window =
            test_support::window(*application,
                                 {.title = "HeliosView2", .width = 320, .height = 240});
    REQUIRE(window != nullptr);

    CHECK(application->window_count() == 1);
    CHECK(window->native_handle() != nullptr);
    CHECK_FALSE(window->is_closed());

    CHECK_NOTHROW(window->show());
    CHECK_NOTHROW(window->hide());
    CHECK_NOTHROW(window->set_title("Renamed"));

    window->close();
    CHECK(window->is_closed());
    CHECK(window->native_handle() == nullptr);
    CHECK(application->window_count() == 0);

    // Closing the last window requests app shutdown (default option).
    std::int32_t exit_code = -1;
    CHECK_NOTHROW(application->run(exit_code));
    CHECK(exit_code == 0);
}

TEST_CASE("a window outlives the app that created it") {
    std::unique_ptr<helios::app> application = test_support::app();

    std::shared_ptr<helios::base_window> window =
            test_support::window(*application, {.title = "Lifetime"});
    REQUIRE(window != nullptr);

    application.reset();
    CHECK(window->is_closed());
    CHECK(window->native_handle() == nullptr);

    window.reset();
}

TEST_CASE("window operations report the wrong thread") {
    std::unique_ptr<helios::app> application = test_support::app();
    std::shared_ptr<helios::base_window> window =
            test_support::window(*application, {.title = "Thread"});
    REQUIRE(window != nullptr);

    std::thread worker([&] {
        try {
            window->show();
            FAIL("show should have thrown");
        } catch (const helios::error& failure) {
            CHECK(failure.code() == helios::wrong_thread);
            CHECK(failure.frame().context == "showing the window");
        }
    });
    worker.join();

    CHECK_NOTHROW(window->close());
}
#endif
