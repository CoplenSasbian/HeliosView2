#pragma once

// Platform application backend. app (App.h) is a handle over this interface;
// each platform implements it in src/<backend>/.

#include <HeliosView2/App.h>

#include <cstdint>
#include <functional>
#include <memory>

namespace helios::detail {

class window_impl;

class app_impl {
public:
    app_impl() = default;
    app_impl(const app_impl&) = delete;
    app_impl& operator=(const app_impl&) = delete;
    virtual ~app_impl() = default;

    virtual void run(std::int32_t& exit_code) = 0;
    virtual void quit() noexcept = 0;
    [[nodiscard]] virtual bool is_running() const noexcept = 0;
    [[nodiscard]] virtual std::uint64_t ui_thread_id() const noexcept = 0;

    [[nodiscard]] virtual std::uint64_t window_count() const = 0;
    virtual void close_all_windows() = 0;

    virtual void post(std::function<void()> work,
                      std::function<void(error)> on_failure) = 0;

    // Window bookkeeping; window_impl calls these.
    virtual void add_window(window_impl& window) = 0;
    virtual void remove_window(window_impl& window) noexcept = 0;
    virtual void window_destroyed(window_impl& window) noexcept = 0;
};

[[nodiscard]] std::unique_ptr<app_impl> create_app_impl(const app_options& options);

}  // namespace helios::detail
