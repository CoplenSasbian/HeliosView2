#pragma once

// The backends behind the public handles. They live inside the DLL and are the
// only place that stays virtual: app and base_window have one backend per
// platform, and the log module has one per caller, which is exactly the case
// where virtual dispatch pays for itself.

#include <HeliosView2/App.h>
#include <HeliosView2/Window.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <utility>

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

    virtual std::shared_ptr<base_window> create_window(const window_options& options) = 0;

    [[nodiscard]] virtual std::uint64_t window_count() const = 0;
    virtual void close_all_windows() = 0;

    virtual void post(std::function<void()> work,
                      std::function<void(error)> on_failure) = 0;

    /// The native window is gone; the app drops its reference.
    virtual void window_destroyed(window_impl& window) noexcept = 0;
};

struct window_impl {
    window_impl() = default;
    window_impl(const window_impl&) = delete;
    window_impl& operator=(const window_impl&) = delete;
    virtual ~window_impl() = default;

    virtual void show() = 0;
    virtual void hide() = 0;
    virtual void close() = 0;
    virtual void set_title(std::string_view title) = 0;

    [[nodiscard]] virtual bool is_closed() const = 0;
    [[nodiscard]] virtual void* native_handle() const = 0;

    /// The owning app is going away; stop notifying it.
    virtual void detach() noexcept = 0;

    /// The handle keeps the window alive for as long as the caller holds it; the
    /// backend only refers to it weakly so a message can safely lock it.
    void attach(std::weak_ptr<base_window> handle) noexcept { m_handle = std::move(handle); }
    [[nodiscard]] std::shared_ptr<base_window> lock_handle() const noexcept {
        return m_handle.lock();
    }

    std::weak_ptr<base_window> m_handle;
    bool m_should_close = true;
};

/// The only code allowed to put a public handle around a backend.
struct app_access {
    [[nodiscard]] static std::unique_ptr<app> make_app(std::unique_ptr<app_impl> backend) {
        return std::unique_ptr<app>{new app(std::move(backend))};
    }

    [[nodiscard]] static std::shared_ptr<base_window> make_window(
            std::unique_ptr<window_impl> backend) {
        auto handle = std::shared_ptr<base_window>{new base_window(std::move(backend))};
        handle->m_impl->attach(handle);
        return handle;
    }

    [[nodiscard]] static window_impl* impl_of(base_window& window) noexcept {
        return window.m_impl.get();
    }
};

/// Platform entry point behind helios::create_app.
[[nodiscard]] std::unique_ptr<app> create_platform_app(const app_options& options);

}  // namespace helios::detail
