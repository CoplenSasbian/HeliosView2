#pragma once

// Public application interface.
//
// app is a handle: the platform implementation lives in the DLL behind a pimpl
// (detail::app_impl), so this header carries declarations only. There is no
// frozen ABI - the DLL and the application are built with the same compiler,
// standard library and flags - but keeping the layout out of the header keeps
// the interface small and lets the implementation evolve freely.
//
// Windows are user derived classes (see Window.h); app is just the message loop
// and the UI thread task queue, and registers windows for their lifetime
// without owning them.

#include <HeliosView2/Error.h>
#include <HeliosView2/HeliosView2_export.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace helios {

class base_window;

namespace detail {
class app_impl;
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
    /// Creates the app and binds it to the calling thread, which becomes the UI
    /// thread. Throws helios::error with the full detail on failure.
    ///
    ///     helios::app application(helios::app_options{.id = "com.example.editor",
    ///                                                .name = "Editor",
    ///                                                .version = "0.1.0"});
    explicit app(const app_options& options = {});

    app(const app&) = delete;
    app& operator=(const app&) = delete;

    /// Moving transfers the platform application; the moved-from handle is
    /// empty and can only be destroyed or assigned to.
    app(app&& other) noexcept;
    app& operator=(app&& other) noexcept;

    ~app();

    /// Runs the message loop on the calling thread until quit() is requested.
    void run(std::int32_t& exit_code);

    void quit() noexcept;

    [[nodiscard]] bool is_running() const noexcept;
    [[nodiscard]] std::uint64_t ui_thread_id() const noexcept;

    [[nodiscard]] std::uint64_t window_count() const;
    void close_all_windows();

    /// Queues work for the UI thread. `on_failure`, when set, receives what a
    /// throwing task could not return.
    void post(std::function<void()> work, std::function<void(error)> on_failure = {});

private:
    friend class base_window;

    [[nodiscard]] detail::app_impl& impl() const;

    std::unique_ptr<detail::app_impl> m_impl;
};

}  // namespace helios
