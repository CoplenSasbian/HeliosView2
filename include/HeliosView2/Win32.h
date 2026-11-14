#pragma once

#if !defined(_WIN32)
#    error "HeliosView2/Win32.h is only available on Windows"
#endif

// Windows specific window extension points.
//
// The portable interface (Window.h) only knows typed events. This header is the
// one place that names WM_* ids and the WPARAM/LPARAM tuple; include it when a
// window has to see raw window procedure messages.

#include <HeliosView2/Platform.h>
#include <HeliosView2/Window.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

namespace helios::win32 {

/// One raw window procedure message, offered after the typed event was emitted.
/// Set `handled` and `result` to keep DefWindowProc from running.
struct native_message {
    void* hwnd = nullptr;   // same value as base_window::native_handle()
    std::uint32_t id = 0;   // WM_*
    std::uintptr_t wparam = 0;
    std::intptr_t lparam = 0;
    std::intptr_t result = 0;
    bool handled = false;
};

using native_message_handler = std::function<void(native_message&)>;

namespace detail {

class handler_extension final : public helios::detail::window_extension {
public:
    handler_extension(helios::base_window& window, native_message_handler handler)
        : m_window(&window), m_handler(std::move(handler)) {}

    bool handle_message(std::uint32_t id, std::uintptr_t argument1,
                        std::intptr_t argument2, std::intptr_t& result) override {
        native_message message{};
        message.hwnd = m_window->native_handle();
        message.id = id;
        message.wparam = argument1;
        message.lparam = argument2;

        try {
            m_handler(message);
        } catch (...) {
            return false;
        }

        if (!message.handled) return false;
        result = message.result;
        return true;
    }

private:
    helios::base_window* m_window;
    native_message_handler m_handler;
};

}  // namespace detail

/// Installs (or clears, with an empty handler) the raw Win32 message hook. The
/// typed events are emitted first; this hook sees every message afterwards,
/// before DefWindowProc. Call it on the UI thread.
inline void set_native_message_handler(helios::base_window& window,
                                       native_message_handler handler) {
    if (!handler) {
        helios::detail::window_access::set_extension(window, nullptr);
        return;
    }

    helios::detail::window_access::set_extension(
            window,
            std::make_shared<detail::handler_extension>(window, std::move(handler)));
}

}  // namespace helios::win32
