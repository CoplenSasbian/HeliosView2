#pragma once

// Platform window backend. base_window (Window.h) is a handle over this
// interface; each platform implements it in src/<backend>/.

#include <HeliosView2/Event.h>
#include <HeliosView2/Platform.h>
#include <HeliosView2/Window.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace helios::detail {

class app_impl;

class window_impl {
public:
    window_impl(app_impl& host, const window_options& options);
    window_impl(const window_impl&) = delete;
    window_impl& operator=(const window_impl&) = delete;
    virtual ~window_impl();

    // Common state shared by every backend.

    void set_event_handler(std::function<void(const event_message&)> handler);
    void set_extension(std::shared_ptr<window_extension> extension);

    void set_should_close(bool should_close) noexcept;
    [[nodiscard]] bool should_close() const noexcept;
    [[nodiscard]] bool is_closed() const noexcept;
    [[nodiscard]] void* native_handle() const noexcept;

    void set_min_size(window_extent size);
    void set_max_size(window_extent size);
    [[nodiscard]] window_extent min_size() const noexcept;
    [[nodiscard]] window_extent max_size() const noexcept;

    [[nodiscard]] double opacity() const noexcept;
    [[nodiscard]] bool resizable() const noexcept;
    [[nodiscard]] bool is_always_on_top() const noexcept;
    [[nodiscard]] double scale() const;

    // Backend helpers.

    void set_native_handle(void* native) noexcept;
    void set_opacity_value(double opacity) noexcept;
    void set_resizable_value(bool resizable) noexcept;
    void set_always_on_top_value(bool on) noexcept;

    /// The native window is going away: mark the window closed and unregister
    /// it from the app.
    void native_lost() noexcept;

    /// The native window is being destroyed but still exists (WM_DESTROY).
    void mark_closed() noexcept;

    /// The app is going away; stop calling back into it.
    void detach_host() noexcept;

    /// Offers a raw platform tuple to the extension attached through a platform
    /// header (HeliosView2/Win32.h and friends).
    [[nodiscard]] bool handle_extension(std::uint32_t id, std::uintptr_t argument1,
                                        std::intptr_t argument2,
                                        std::intptr_t& result) noexcept;

    /// Publishes one typed event to the installed handler. A handler must not
    /// throw across the platform callback, so the exception is contained here.
    template <class Event>
    void emit(const Event& event) noexcept {
        if (!m_event_handler) return;
        try {
            const event_message message{Event::kind, &event, sizeof(Event)};
            m_event_handler(message);
        } catch (...) {
        }
    }

    // Platform operations.

    virtual void show() = 0;
    virtual void hide() = 0;
    [[nodiscard]] virtual bool is_visible() const = 0;

    virtual void set_title(std::string_view title) = 0;
    [[nodiscard]] virtual std::string title() const = 0;

    virtual void set_size(std::int32_t width, std::int32_t height) = 0;
    [[nodiscard]] virtual window_extent size() const = 0;
    [[nodiscard]] virtual window_extent client_size() const = 0;

    virtual void set_position(std::int32_t x, std::int32_t y) = 0;
    [[nodiscard]] virtual std::pair<std::int32_t, std::int32_t> position() const = 0;
    virtual void center() = 0;

    virtual void minimize() = 0;
    virtual void maximize() = 0;
    virtual void restore() = 0;
    [[nodiscard]] virtual bool is_minimized() const = 0;
    [[nodiscard]] virtual bool is_maximized() const = 0;

    virtual void focus() = 0;
    [[nodiscard]] virtual bool is_focused() const = 0;
    virtual void activate() = 0;

    virtual void set_always_on_top(bool on) = 0;
    virtual void set_opacity(double opacity) = 0;
    virtual void enable(bool enabled) = 0;
    [[nodiscard]] virtual bool is_enabled() const = 0;
    virtual void request_attention() = 0;
    virtual void set_resizable(bool resizable) = 0;

    [[nodiscard]] virtual std::uint32_t dpi() const = 0;

    virtual void close() = 0;
    virtual void destroy_native() noexcept = 0;

protected:
    [[nodiscard]] app_impl& host() const noexcept { return *m_host; }

private:
    app_impl* m_host{nullptr};
    void* m_native{nullptr};
    std::function<void(const event_message&)> m_event_handler;
    std::shared_ptr<window_extension> m_extension;
    window_extent m_min_size{};
    window_extent m_max_size{};
    double m_opacity{1.0};
    bool m_resizable{true};
    bool m_always_on_top{false};
    std::atomic<bool> m_should_close{true};
    std::atomic<bool> m_closed{false};
};

[[nodiscard]] std::unique_ptr<window_impl> create_window_impl(
        app_impl& host, const window_options& options);

}  // namespace helios::detail
