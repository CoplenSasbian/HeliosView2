#include <HeliosView2/App.h>

#include "test_support.h"

#include <doctest/doctest.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#if defined(_WIN32)
TEST_CASE("create_app returns a usable application") {
    std::unique_ptr<helios::app> application = test_support::app();
    REQUIRE(application != nullptr);

    CHECK(application->ui_thread_id() != 0);
    CHECK_FALSE(application->is_running());
    CHECK(application->window_count() == 0);
}

TEST_CASE("run exits when quit is called from another thread") {
    std::unique_ptr<helios::app> application = test_support::app();
    REQUIRE(application != nullptr);

    std::atomic<bool> quit_called{false};
    std::thread worker([&] {
        while (!application->is_running()) std::this_thread::yield();
        application->quit();
        quit_called.store(true);
    });

    std::int32_t exit_code = -1;
    application->run(exit_code);
    worker.join();

    CHECK(quit_called.load());
    CHECK(exit_code == 0);
    CHECK_FALSE(application->is_running());
}

TEST_CASE("posted tasks run on the ui thread") {
    std::unique_ptr<helios::app> application = test_support::app();
    REQUIRE(application != nullptr);

    const std::thread::id ui_thread = std::this_thread::get_id();
    bool ran = false;
    std::thread::id ran_on{};

    application->post([&] {
        ran = true;
        ran_on = std::this_thread::get_id();
        application->quit();
    });

    std::int32_t exit_code = -1;
    application->run(exit_code);

    CHECK(ran);
    CHECK(ran_on == ui_thread);
    CHECK(exit_code == 0);
}

TEST_CASE("tasks can be posted from another thread while running") {
    std::unique_ptr<helios::app> application = test_support::app();
    REQUIRE(application != nullptr);

    std::atomic<bool> ran{false};

    std::thread worker([&] {
        while (!application->is_running()) std::this_thread::yield();
        try {
            application->post([&] {
                ran.store(true);
                application->quit();
            });
        } catch (const helios::error&) {
        }
    });

    std::int32_t exit_code = -1;
    application->run(exit_code);
    worker.join();

    CHECK(ran.load());
    CHECK(exit_code == 0);
}

TEST_CASE("a throwing task reports to its handler") {
    std::unique_ptr<helios::app> application = test_support::app();
    REQUIRE(application != nullptr);

    helios::error reported;
    bool called = false;
    std::thread::id handler_thread{};

    application->post(
            [] { throw std::runtime_error("boom"); },
            [&](helios::error failure) {
                called = true;
                handler_thread = std::this_thread::get_id();
                reported = std::move(failure);
                application->quit();
            });

    std::int32_t exit_code = -1;
    application->run(exit_code);

    CHECK(called);
    CHECK(handler_thread == std::this_thread::get_id());
    CHECK(reported.code() == helios::unhandled_exception);
    CHECK(reported.frame().message.find("boom") != std::string::npos);
}

TEST_CASE("post rejects an empty task") {
    std::unique_ptr<helios::app> application = test_support::app();
    REQUIRE(application != nullptr);

    try {
        application->post({});
        FAIL("post should have thrown");
    } catch (const helios::error& failure) {
        CHECK(failure.code() == helios::invalid_argument);
    }
}

TEST_CASE("run rejects the wrong thread") {
    std::unique_ptr<helios::app> application = test_support::app();
    REQUIRE(application != nullptr);

    std::thread worker([&] {
        try {
            std::int32_t exit_code = -1;
            application->run(exit_code);
            FAIL("run should have thrown");
        } catch (const helios::error& failure) {
            CHECK(failure.code() == helios::wrong_thread);
        }
    });
    worker.join();

    application->quit();
    std::int32_t exit_code = -1;
    CHECK_NOTHROW(application->run(exit_code));
}
#endif
