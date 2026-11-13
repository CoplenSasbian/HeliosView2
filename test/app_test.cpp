#include <HeliosView2/App.h>
#include <HeliosView2/Task.h>

#include <doctest/doctest.h>

#include <atomic>
#include <thread>

namespace abi = helios::abi;

namespace {

class fake_app final : public helios::app {
public:
    fake_app() noexcept = default;

    std::uint32_t add_ref() noexcept override { return ++refs; }

    std::uint32_t release() noexcept override {
        const auto remaining = --refs;
        if (remaining == 0) delete this;
        return remaining;
    }

    abi::status query_interface(const abi::guid& id, void** out) noexcept override {
        if (out == nullptr) return abi::invalid_argument;
        *out = nullptr;

        if (id == helios::app::iid) {
            *out = static_cast<helios::app*>(this);
            add_ref();
            return abi::ok;
        }
        if (id == abi::unknown_iid) {
            *out = static_cast<abi::unknown*>(this);
            add_ref();
            return abi::ok;
        }
        return abi::no_interface;
    }

    abi::status run(std::int32_t* exit_code) noexcept override {
        running = true;
        if (exit_code != nullptr) *exit_code = 0;
        return abi::ok;
    }

    void quit() noexcept override { running = false; }
    bool is_running() const noexcept override { return running; }
    std::uint64_t ui_thread_id() const noexcept override { return 1; }

    abi::status create_window(const abi::window_options*, helios::base_window** out) noexcept override {
        if (out != nullptr) *out = nullptr;
        return abi::unsupported;
    }

    abi::status post(abi::task*) noexcept override { return abi::unsupported; }

    std::uint64_t window_count() const noexcept override { return 0; }

private:
    std::atomic<std::uint32_t> refs{1};
    bool running = false;
};

}  // namespace

TEST_CASE("app identifier is derived from its stable name") {
    static_assert(helios::app::iid == abi::make_guid("helios.abi.app/1"));
    static_assert((helios::app::iid.data3 >> 12) == 5);
    static_assert((helios::app::iid.data4[0] & 0xC0) == 0x80);
    static_assert(helios::app::iid != abi::make_guid("helios.abi.app/2"));
}

TEST_CASE("app is an implementable interface") {
    fake_app implementation;
    helios::app& application = implementation;

    CHECK_FALSE(application.is_running());
    CHECK(application.window_count() == 0);
    CHECK(application.ui_thread_id() == 1);

    std::int32_t exit_code = -1;
    CHECK(abi::succeeded(application.run(&exit_code)));
    CHECK(exit_code == 0);
    CHECK(application.is_running());

    application.quit();
    CHECK_FALSE(application.is_running());
}

TEST_CASE("app can be queried through the interface identifier") {
    fake_app implementation;

    auto application = abi::query<helios::app>(implementation);
    REQUIRE(application != nullptr);
    CHECK_FALSE(application->is_running());
}

#if defined(_WIN32)
TEST_CASE("create_app returns a win32 application") {
    helios::app_options options{};
    abi::ptr<helios::app> application;

    const abi::status status = helios::create_app(&options, std::out_ptr(application));
    REQUIRE(abi::succeeded(status));
    REQUIRE(application != nullptr);

    CHECK(application->ui_thread_id() != 0);
    CHECK_FALSE(application->is_running());
    CHECK(application->window_count() == 0);

    helios::base_window* window = nullptr;
    CHECK(application->create_window(nullptr, &window) == abi::invalid_argument);
    CHECK(window == nullptr);

    application->quit();

    std::int32_t exit_code = -1;
    CHECK(abi::succeeded(application->run(&exit_code)));
    CHECK(exit_code == 0);
    CHECK_FALSE(application->is_running());
}

TEST_CASE("run exits when quit is called from another thread") {
    helios::app_options options{};
    abi::ptr<helios::app> application;

    REQUIRE(abi::succeeded(helios::create_app(&options, std::out_ptr(application))));
    REQUIRE(application != nullptr);

    std::atomic<bool> quit_called{false};
    std::thread worker([&] {
        while (!application->is_running()) std::this_thread::yield();
        application->quit();
        quit_called.store(true);
    });

    std::int32_t exit_code = -1;
    CHECK(abi::succeeded(application->run(&exit_code)));
    worker.join();

    CHECK(quit_called.load());
    CHECK(exit_code == 0);
    CHECK_FALSE(application->is_running());
}

TEST_CASE("posted tasks run on the ui thread") {
    helios::app_options options{};
    abi::ptr<helios::app> application;

    REQUIRE(abi::succeeded(helios::create_app(&options, std::out_ptr(application))));
    REQUIRE(application != nullptr);

    const std::thread::id ui_thread = std::this_thread::get_id();
    bool ran = false;
    std::thread::id ran_on{};

    CHECK(abi::succeeded(helios::post(*application, [&] {
        ran = true;
        ran_on = std::this_thread::get_id();
        application->quit();
    })));

    std::int32_t exit_code = -1;
    CHECK(abi::succeeded(application->run(&exit_code)));

    CHECK(ran);
    CHECK(ran_on == ui_thread);
    CHECK(exit_code == 0);
}

TEST_CASE("tasks can be posted from another thread while running") {
    helios::app_options options{};
    abi::ptr<helios::app> application;

    REQUIRE(abi::succeeded(helios::create_app(&options, std::out_ptr(application))));
    REQUIRE(application != nullptr);

    std::atomic<bool> posted{false};
    std::atomic<bool> ran{false};

    std::thread worker([&] {
        while (!application->is_running()) std::this_thread::yield();
        const abi::status status = helios::post(*application, [&] {
            ran.store(true);
            application->quit();
        });
        posted.store(abi::succeeded(status));
    });

    std::int32_t exit_code = -1;
    CHECK(abi::succeeded(application->run(&exit_code)));
    worker.join();

    CHECK(posted.load());
    CHECK(ran.load());
    CHECK(exit_code == 0);
}
#endif
