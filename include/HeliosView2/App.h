#pragma once

// Public application interface.
//
// The class is a handle: the platform implementation lives in the DLL behind a
// pimpl (helios::detail::app_impl), so this header stays small, the class layout
// can change without touching call sites, and app::run/quit are plain calls
// rather than virtual dispatch. What does stay virtual is the part that genuinely
// has several implementations - the platform backend inside the DLL.
//
// Plain C++ on purpose: the DLL exports ordinary classes and functions, so the
// headers and the binary are built together with the same compiler, standard
// library and flags. Ship a binary per toolchain rather than freeze an ABI.

#include <HeliosView2/Error.h>
#include <HeliosView2/HeliosView2_export.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace helios {

struct window_options;  // Window.h
class base_window;
class app;

namespace detail {
class app_impl;
struct app_access;
}  // namespace detail

/// Options for creating an application.
struct app_options {
    std::string id;
    std::string name;
    std::string version;
    bool quit_on_last_window_closed = true;
};

class HELIOSVIEW2_EXPORT app {
public:
    app(const app&) = delete;
    app& operator=(const app&) = delete;
    ~app();

    /// Runs the message loop on the calling thread until quit() is requested.
    void run(std::int32_t& exit_code);

    void quit() noexcept;

    [[nodiscard]] bool is_running() const noexcept;
    [[nodiscard]] std::uint64_t ui_thread_id() const noexcept;

    /// Creates a window; must be called on the UI thread.
    std::shared_ptr<base_window> create_window(const window_options& options);

    [[nodiscard]] std::uint64_t window_count() const;
    void close_all_windows();

    /// Queues work for the UI thread. `on_failure`, when set, receives what a
    /// throwing task could not return.
    void post(std::function<void()> work, std::function<void(error)> on_failure = {});

private:
    friend struct detail::app_access;

    explicit app(std::unique_ptr<detail::app_impl> impl) noexcept;

    std::unique_ptr<detail::app_impl> m_impl;
};

/// Creates an application; throws helios::error with the full detail on failure.
///
///     auto application = helios::create_app({.id = "com.example.editor",
///                                           .name = "Editor",
///                                           .version = "0.1.0"});
[[nodiscard]] HELIOSVIEW2_EXPORT std::unique_ptr<app> create_app(
        const app_options& options);

}  // namespace helios
