#pragma once

// Public window interface.
//
// base_window is a handle: the platform implementation lives in the DLL behind
// a pimpl (detail::window_impl), so this header carries declarations only.
//
// Events are delivered to the handler installed with set_event_handler(); the
// handler takes an event_message and decodes it. A window can still be derived
// for annotated member functions, which handler_scanner (HandlerScanner.h)
// turns into an event handler for you:
//
//     class editor_window final : public helios::base_window {
//     public:
//         using base_window::base_window;
//
//         [[= helios::event_handler]]
//         void on_resize(const helios::resize_event& event) { ... }
//     };
//
// Nothing here is platform specific. A platform that needs to expose its own
// message ids or arguments does it in a platform header (on Windows:
// HeliosView2/Win32.h) through HeliosView2/Platform.h.
//
// There is no app-level window factory: a window is constructed where it is
// used, on the app's UI thread, and unregisters itself when it is destroyed.

#include <HeliosView2/Event.h>
#include <HeliosView2/HeliosView2_export.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace helios {

class app;
class base_window;

namespace detail {
class window_impl;
struct window_access;
}  // namespace detail

struct window_extent {
    std::int32_t width = 0;
    std::int32_t height = 0;

    bool operator==(const window_extent&) const = default;
};

/// The options a caller actually writes. An unset x/y lets the platform place
/// the window.
struct window_options {
    std::string title;
    std::int32_t width = 800;
    std::int32_t height = 600;
    std::optional<std::int32_t> x;
    std::optional<std::int32_t> y;
    bool resizable = true;
    bool visible = false;
    bool centered = false;
    bool always_on_top = false;
};

class HELIOSVIEW2_EXPORT base_window {
public:
    /// Creates the native window on the calling thread, which must be the app's
    /// UI thread. The window registers itself with the app until it is
    /// destroyed. Throws helios::error on failure.
    base_window(app& host, const window_options& options = {});

    base_window(const base_window&) = delete;
    base_window& operator=(const base_window&) = delete;

    /// Moving transfers the native window and its backend state; the
    /// moved-from handle is empty and can only be destroyed or assigned to.
    ///
    /// Move a window before installing a handler: a handler_scanner (or a
    /// callback) that captures the handle keeps pointing at the old address.
    base_window(base_window&& other) noexcept;
    base_window& operator=(base_window&& other) noexcept;

    virtual ~base_window();

    // Visibility and title.

    void show();
    void hide();
    [[nodiscard]] bool is_visible() const;

    void set_title(std::string_view title);
    [[nodiscard]] std::string title() const;

    // Geometry. Sizes and positions are in physical pixels and refer to the
    // whole window (frame included); client_size() is the drawable area.

    void set_size(std::int32_t width, std::int32_t height);
    [[nodiscard]] window_extent size() const;
    [[nodiscard]] window_extent client_size() const;

    void set_position(std::int32_t x, std::int32_t y);
    [[nodiscard]] std::pair<std::int32_t, std::int32_t> position() const;

    /// Centres the window on the work area of its monitor.
    void center();

    // Window state.

    void minimize();
    void maximize();
    void restore();
    [[nodiscard]] bool is_minimized() const;
    [[nodiscard]] bool is_maximized() const;

    void focus();
    [[nodiscard]] bool is_focused() const;

    /// Raises and activates the window, as if the user had clicked its taskbar
    /// button.
    void activate();

    // Appearance and input.

    void set_always_on_top(bool on);
    /// The requested state; a hidden window becomes topmost when it is shown.
    [[nodiscard]] bool is_always_on_top() const;

    /// 0.0 is fully transparent, 1.0 fully opaque.
    void set_opacity(double opacity);
    [[nodiscard]] double opacity() const;

    void enable(bool enabled);
    [[nodiscard]] bool is_enabled() const;

    /// Flashes the taskbar button until the window is activated.
    void request_attention();

    /// Whether the frame can be resized by the user.
    void set_resizable(bool resizable);
    [[nodiscard]] bool resizable() const;

    /// Track-size limits in physical pixels; a zero extent clears the limit.
    void set_min_size(window_extent size);
    void set_max_size(window_extent size);
    [[nodiscard]] window_extent min_size() const;
    [[nodiscard]] window_extent max_size() const;

    // DPI.

    [[nodiscard]] std::uint32_t dpi() const;
    [[nodiscard]] double scale() const;

    // Lifecycle.

    void close();
    void set_should_close(bool should_close);
    [[nodiscard]] bool should_close() const;
    [[nodiscard]] bool is_closed() const;
    [[nodiscard]] void* native_handle() const;

    /// Installs (or clears) the handler that receives every window event. The
    /// message borrows its payload for the duration of the call, so copy it out
    /// if it has to outlive the handler. Call this on the UI thread.
    void set_event_handler(std::function<void(const event_message&)> handler);

private:
    friend struct detail::window_access;

    [[nodiscard]] detail::window_impl& impl() const;

    std::unique_ptr<detail::window_impl> m_impl;
};

}  // namespace helios
