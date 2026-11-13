#include <HeliosView2/Execution.h>
#include <HeliosView2/UiScheduler.h>

#include "test_support.h"

#include <doctest/doctest.h>

#include <atomic>
#include <cstdint>
#include <thread>
#include <tuple>

namespace ex = helios::exec;

#if defined(_WIN32)
TEST_CASE("ui_scheduler delivers values on the ui thread") {
    std::unique_ptr<helios::app> application = test_support::app();

    const std::thread::id ui_thread = std::this_thread::get_id();
    std::atomic<bool> on_ui{false};
    std::atomic<bool> completed{false};
    std::atomic<int> value{0};

    std::thread worker([&] {
        while (!application->is_running()) std::this_thread::yield();

        auto result = helios::sync_wait(
                ex::then(helios::ui_scheduler{*application}.schedule(), [&] {
                    on_ui.store(std::this_thread::get_id() == ui_thread);
                    return 42;
                }));

        if (result.has_value()) {
            value.store(std::get<0>(result.value()));
            completed.store(true);
        }
        application->quit();
    });

    std::int32_t exit_code = -1;
    CHECK_NOTHROW(application->run(exit_code));
    worker.join();

    CHECK(completed.load());
    CHECK(on_ui.load());
    CHECK(value.load() == 42);
    CHECK(exit_code == 0);
}
#endif
