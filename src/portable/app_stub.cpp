#include "../AppImpl.h"

#include <HeliosView2/App.h>

namespace helios::detail {

namespace {

[[noreturn]] void no_backend(const char* context) {
    throw make_error(unsupported, context, "this build has no windowing backend");
}

class stub_app_impl final : public app_impl {
public:
    explicit stub_app_impl(const app_options&) {
        no_backend("creating the application");
    }

    void run(std::int32_t&) override {
        no_backend("running the message loop");
    }

    void quit() noexcept override {}

    bool is_running() const noexcept override {
        return false;
    }

    std::uint64_t ui_thread_id() const noexcept override {
        return 0;
    }

    std::uint64_t window_count() const override {
        return 0;
    }

    void close_all_windows() override {}

    void post(std::function<void()>, std::function<void(error)>) override {
        no_backend("posting a task");
    }

    void add_window(window_impl&) override {}

    void remove_window(window_impl&) noexcept override {}

    void window_destroyed(window_impl&) noexcept override {}
};

}  // namespace

std::unique_ptr<app_impl> create_app_impl(const app_options& options) {
    return std::make_unique<stub_app_impl>(options);
}

}  // namespace helios::detail
