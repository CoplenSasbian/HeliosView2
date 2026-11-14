#include "../AppImpl.h"
#include "../WindowImpl.h"

#include <HeliosView2/App.h>
#include <HeliosView2/Event.h>
#include <HeliosView2/Window.h>

#ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#    define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

namespace helios::detail {

class win32_window_impl;

namespace {

LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam,
                             LPARAM lparam) noexcept;

constexpr wchar_t kWindowClass[] = L"HeliosView2.Window";

std::once_flag g_register_once;
ATOM g_class_atom = 0;

std::wstring to_wide(std::string_view utf8) {
    if (utf8.empty()) return {};

    const int length = ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                                             static_cast<int>(utf8.size()), nullptr, 0);
    if (length <= 0) return {};

    std::wstring result(static_cast<std::size_t>(length), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                          result.data(), length);
    return result;
}

std::string to_utf8(std::wstring_view wide) {
    if (wide.empty()) return {};

    const int size = ::WideCharToMultiByte(CP_UTF8, 0, wide.data(),
                                           static_cast<int>(wide.size()), nullptr, 0,
                                           nullptr, nullptr);
    if (size <= 0) return {};

    std::string result(static_cast<std::size_t>(size), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                          result.data(), size, nullptr, nullptr);
    return result;
}

template <class Function>
Function load_proc(HMODULE module, const char* name) noexcept {
    return reinterpret_cast<Function>(reinterpret_cast<void*>(::GetProcAddress(module, name)));
}

/// " (error 5: Access is denied.)" - the platform turns its own codes into text
/// here, so the error type never carries platform specific fields.
std::string platform_text(std::int32_t code) {
    if (code == 0) return {};

    wchar_t* buffer = nullptr;
    const DWORD length = ::FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                    | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, static_cast<DWORD>(code), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);

    std::string text;
    if (length != 0 && buffer != nullptr) {
        const int size = ::WideCharToMultiByte(CP_UTF8, 0, buffer, static_cast<int>(length),
                                               nullptr, 0, nullptr, nullptr);
        if (size > 0) {
            text.resize(static_cast<std::size_t>(size));
            ::WideCharToMultiByte(CP_UTF8, 0, buffer, static_cast<int>(length), text.data(),
                                  size, nullptr, nullptr);
        }
    }
    if (buffer != nullptr) ::LocalFree(buffer);

    while (!text.empty() && (text.back() == '\r' || text.back() == '\n' || text.back() == ' ')) {
        text.pop_back();
    }

    std::string suffix = " (error " + std::to_string(code);
    if (!text.empty()) {
        suffix += ": ";
        suffix += text;
    }
    suffix += ')';
    return suffix;
}

std::uint32_t window_dpi(HWND hwnd) noexcept {
    using get_dpi_fn = UINT(WINAPI*)(HWND);
    const HMODULE user32 = ::GetModuleHandleW(L"user32.dll");
    if (user32 != nullptr) {
        const auto get_dpi = load_proc<get_dpi_fn>(user32, "GetDpiForWindow");
        if (get_dpi != nullptr) {
            const UINT dpi = get_dpi(hwnd);
            if (dpi != 0) return dpi;
        }
    }
    return 96;  // no per-monitor DPI support: report the classic scale
}

void center_window(HWND hwnd) noexcept {
    RECT rect{};
    if (!::GetWindowRect(hwnd, &rect)) return;

    const HMONITOR monitor = ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (monitor == nullptr || !::GetMonitorInfoW(monitor, &info)) return;

    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    const int x = info.rcWork.left + ((info.rcWork.right - info.rcWork.left) - width) / 2;
    const int y = info.rcWork.top + ((info.rcWork.bottom - info.rcWork.top) - height) / 2;

    ::SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

key_modifier current_modifiers() noexcept {
    key_modifier modifiers = key_modifier::none;
    if ((::GetKeyState(VK_SHIFT) & 0x8000) != 0) modifiers |= key_modifier::shift;
    if ((::GetKeyState(VK_CONTROL) & 0x8000) != 0) modifiers |= key_modifier::control;
    if ((::GetKeyState(VK_MENU) & 0x8000) != 0) modifiers |= key_modifier::alt;
    if ((::GetKeyState(VK_LWIN) & 0x8000) != 0
        || (::GetKeyState(VK_RWIN) & 0x8000) != 0) {
        modifiers |= key_modifier::super;
    }
    if ((::GetKeyState(VK_CAPITAL) & 0x0001) != 0) modifiers |= key_modifier::caps_lock;
    if ((::GetKeyState(VK_NUMLOCK) & 0x0001) != 0) modifiers |= key_modifier::num_lock;
    return modifiers;
}

key_modifier mouse_modifiers(WPARAM wparam) noexcept {
    key_modifier modifiers = key_modifier::none;
    if ((wparam & MK_SHIFT) != 0) modifiers |= key_modifier::shift;
    if ((wparam & MK_CONTROL) != 0) modifiers |= key_modifier::control;
    if ((::GetKeyState(VK_MENU) & 0x8000) != 0) modifiers |= key_modifier::alt;
    if ((::GetKeyState(VK_CAPITAL) & 0x0001) != 0) modifiers |= key_modifier::caps_lock;
    if ((::GetKeyState(VK_NUMLOCK) & 0x0001) != 0) modifiers |= key_modifier::num_lock;
    return modifiers;
}

std::int32_t point_x(LPARAM value) noexcept {
    return static_cast<std::int32_t>(static_cast<std::int16_t>(LOWORD(value)));
}

std::int32_t point_y(LPARAM value) noexcept {
    return static_cast<std::int32_t>(static_cast<std::int16_t>(HIWORD(value)));
}

bool ensure_window_class() noexcept {
    std::call_once(g_register_once, [] {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wc.lpfnWndProc = &window_proc;
        wc.hInstance = ::GetModuleHandleW(nullptr);
        wc.hCursor = ::LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));  // IDC_ARROW
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = kWindowClass;
        g_class_atom = ::RegisterClassExW(&wc);
    });

    if (g_class_atom != 0) return true;

    WNDCLASSEXW existing{};
    existing.cbSize = sizeof(existing);
    return ::GetClassInfoExW(::GetModuleHandleW(nullptr), kWindowClass, &existing) != FALSE;
}

}  // namespace

class win32_window_impl final : public window_impl {
public:
    win32_window_impl(app_impl& host, const window_options& options);
    ~win32_window_impl() override;

    win32_window_impl(const win32_window_impl&) = delete;
    win32_window_impl& operator=(const win32_window_impl&) = delete;

    void show() override;
    void hide() override;
    [[nodiscard]] bool is_visible() const override;

    void set_title(std::string_view title) override;
    [[nodiscard]] std::string title() const override;

    void set_size(std::int32_t width, std::int32_t height) override;
    [[nodiscard]] window_extent size() const override;
    [[nodiscard]] window_extent client_size() const override;
    void set_position(std::int32_t x, std::int32_t y) override;
    [[nodiscard]] std::pair<std::int32_t, std::int32_t> position() const override;
    void center() override;

    void minimize() override;
    void maximize() override;
    void restore() override;
    [[nodiscard]] bool is_minimized() const override;
    [[nodiscard]] bool is_maximized() const override;

    void focus() override;
    [[nodiscard]] bool is_focused() const override;
    void activate() override;

    void set_always_on_top(bool on) override;
    void set_opacity(double opacity) override;
    void enable(bool enabled) override;
    [[nodiscard]] bool is_enabled() const override;
    void request_attention() override;
    void set_resizable(bool resizable) override;

    [[nodiscard]] std::uint32_t dpi() const override;

    void close() override;
    void destroy_native() noexcept override;

    LRESULT dispatch(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) noexcept;

private:
    void create_native(const window_options& options);
    [[nodiscard]] HWND require_native(const char* context) const;
    void require_ui_thread(const char* context) const;

    enum class state : std::uint8_t {
        restored,
        minimized,
        maximized,
    };

    DWORD m_thread_id{};
    bool m_created{false};
    bool m_tracking_mouse{false};
    state m_state{state::restored};
};

win32_window_impl::win32_window_impl(app_impl& host, const window_options& options)
    : window_impl(host, options),
      m_thread_id(static_cast<DWORD>(host.ui_thread_id())) {
    if (::GetCurrentThreadId() != m_thread_id) {
        throw make_error(wrong_thread, "creating a window",
                         "windows must be created on the app's UI thread");
    }

    create_native(options);
}

win32_window_impl::~win32_window_impl() {
    if (auto* native = native_handle()) {
        ::DestroyWindow(static_cast<HWND>(native));
    }
}

void win32_window_impl::create_native(const window_options& options) {
    if (!ensure_window_class()) {
        const auto native = static_cast<std::int32_t>(::GetLastError());
        throw make_error_text(
                platform_failure, "creating the window",
                "RegisterClassExW failed" + platform_text(native));
    }

    const std::wstring title = to_wide(options.title);

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    if (options.resizable) style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
    if (options.visible) style |= WS_VISIBLE;

    const int x = options.x.value_or(CW_USEDEFAULT);
    const int y = options.y.value_or(CW_USEDEFAULT);
    const int width = options.width > 0 ? options.width : 800;
    const int height = options.height > 0 ? options.height : 600;

    const HWND hwnd = ::CreateWindowExW(0, kWindowClass,
                                        title.empty() ? L"HeliosView2" : title.c_str(),
                                        style, x, y, width, height,
                                        nullptr, nullptr, ::GetModuleHandleW(nullptr),
                                        this);
    if (hwnd == nullptr) {
        const auto native = static_cast<std::int32_t>(::GetLastError());
        throw make_error_text(
                platform_failure, "creating the window",
                "CreateWindowExW failed for " + std::to_string(width) + "x"
                        + std::to_string(height) + platform_text(native));
    }

    // Messages sent during CreateWindowExW reached the window procedure before
    // this point; from here on the derived class is constructed for real.
    m_created = true;

    if (is_always_on_top()) {
        ::SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    if (options.centered) center_window(hwnd);
}

HWND win32_window_impl::require_native(const char* context) const {
    auto* native = native_handle();
    if (native == nullptr) {
        throw make_error(invalid_state, context, "the native window is gone");
    }
    return static_cast<HWND>(native);
}

void win32_window_impl::require_ui_thread(const char* context) const {
    if (::GetCurrentThreadId() != m_thread_id) {
        throw make_error(wrong_thread, context,
                         "the window can only be changed on the app's UI thread");
    }
}

void win32_window_impl::show() {
    const HWND hwnd = require_native("showing the window");
    require_ui_thread("showing the window");
    ::ShowWindow(hwnd, SW_SHOW);
    ::UpdateWindow(hwnd);

    // A hidden window cannot be topmost; the request is applied once it is
    // actually visible.
    if (is_always_on_top()) {
        ::SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

void win32_window_impl::hide() {
    const HWND hwnd = require_native("hiding the window");
    require_ui_thread("hiding the window");
    ::ShowWindow(hwnd, SW_HIDE);
}

bool win32_window_impl::is_visible() const {
    const auto* native = native_handle();
    return native != nullptr && ::IsWindowVisible(static_cast<HWND>(const_cast<void*>(native))) != FALSE;
}

void win32_window_impl::set_title(std::string_view title) {
    const HWND hwnd = require_native("setting the window title");
    require_ui_thread("setting the window title");

    const std::wstring wide = to_wide(title);
    if (!::SetWindowTextW(hwnd, wide.c_str())) {
        const auto native = static_cast<std::int32_t>(::GetLastError());
        throw make_error_text(platform_failure, "setting the window title",
                              "SetWindowTextW failed" + platform_text(native));
    }
}

std::string win32_window_impl::title() const {
    const auto* native = native_handle();
    if (native == nullptr) return {};

    const HWND hwnd = static_cast<HWND>(const_cast<void*>(native));
    const int length = ::GetWindowTextLengthW(hwnd);
    if (length <= 0) return {};

    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    const int copied = ::GetWindowTextW(hwnd, text.data(), static_cast<int>(text.size()));
    if (copied <= 0) return {};

    text.resize(static_cast<std::size_t>(copied));
    return to_utf8(text);
}

void win32_window_impl::set_size(std::int32_t width, std::int32_t height) {
    const HWND hwnd = require_native("resizing the window");
    require_ui_thread("resizing the window");
    if (width <= 0 || height <= 0) {
        throw make_error(invalid_argument, "resizing the window",
                         "the width and height must be positive");
    }

    if (!::SetWindowPos(hwnd, nullptr, 0, 0, width, height,
                        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE)) {
        const auto native = static_cast<std::int32_t>(::GetLastError());
        throw make_error_text(platform_failure, "resizing the window",
                              "SetWindowPos failed" + platform_text(native));
    }
}

window_extent win32_window_impl::size() const {
    const auto* native = native_handle();
    if (native == nullptr) return {};

    RECT rect{};
    if (!::GetWindowRect(static_cast<HWND>(const_cast<void*>(native)), &rect)) return {};
    return {rect.right - rect.left, rect.bottom - rect.top};
}

window_extent win32_window_impl::client_size() const {
    const auto* native = native_handle();
    if (native == nullptr) return {};

    RECT rect{};
    if (!::GetClientRect(static_cast<HWND>(const_cast<void*>(native)), &rect)) return {};
    return {rect.right - rect.left, rect.bottom - rect.top};
}

void win32_window_impl::set_position(std::int32_t x, std::int32_t y) {
    const HWND hwnd = require_native("moving the window");
    require_ui_thread("moving the window");

    if (!::SetWindowPos(hwnd, nullptr, x, y, 0, 0,
                        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE)) {
        const auto native = static_cast<std::int32_t>(::GetLastError());
        throw make_error_text(platform_failure, "moving the window",
                              "SetWindowPos failed" + platform_text(native));
    }
}

std::pair<std::int32_t, std::int32_t> win32_window_impl::position() const {
    const auto* native = native_handle();
    if (native == nullptr) return {};

    RECT rect{};
    if (!::GetWindowRect(static_cast<HWND>(const_cast<void*>(native)), &rect)) return {};
    return {rect.left, rect.top};
}

void win32_window_impl::center() {
    const HWND hwnd = require_native("centering the window");
    require_ui_thread("centering the window");
    center_window(hwnd);
}

void win32_window_impl::minimize() {
    const HWND hwnd = require_native("minimizing the window");
    require_ui_thread("minimizing the window");
    ::ShowWindow(hwnd, SW_MINIMIZE);
}

void win32_window_impl::maximize() {
    const HWND hwnd = require_native("maximizing the window");
    require_ui_thread("maximizing the window");
    ::ShowWindow(hwnd, SW_MAXIMIZE);
}

void win32_window_impl::restore() {
    const HWND hwnd = require_native("restoring the window");
    require_ui_thread("restoring the window");
    ::ShowWindow(hwnd, SW_RESTORE);
}

bool win32_window_impl::is_minimized() const {
    const auto* native = native_handle();
    return native != nullptr
            && ::IsIconic(static_cast<HWND>(const_cast<void*>(native))) != FALSE;
}

bool win32_window_impl::is_maximized() const {
    const auto* native = native_handle();
    return native != nullptr
            && ::IsZoomed(static_cast<HWND>(const_cast<void*>(native))) != FALSE;
}

void win32_window_impl::focus() {
    const HWND hwnd = require_native("focusing the window");
    require_ui_thread("focusing the window");
    ::SetFocus(hwnd);
}

bool win32_window_impl::is_focused() const {
    const auto* native = native_handle();
    return native != nullptr
            && ::GetFocus() == static_cast<HWND>(const_cast<void*>(native));
}

void win32_window_impl::activate() {
    const HWND hwnd = require_native("activating the window");
    require_ui_thread("activating the window");
    if (!::SetForegroundWindow(hwnd)) ::BringWindowToTop(hwnd);
}

void win32_window_impl::set_always_on_top(bool on) {
    const HWND hwnd = require_native("changing the window z-order");
    require_ui_thread("changing the window z-order");

    set_always_on_top_value(on);
    // A hidden window cannot become topmost yet; show() applies the request.
    if (on && !::IsWindowVisible(hwnd)) return;

    if (!::SetWindowPos(hwnd, on ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE)) {
        const auto native = static_cast<std::int32_t>(::GetLastError());
        throw make_error_text(platform_failure, "changing the window z-order",
                              "SetWindowPos failed" + platform_text(native));
    }
}

void win32_window_impl::set_opacity(double opacity) {
    const HWND hwnd = require_native("changing the window opacity");
    require_ui_thread("changing the window opacity");
    if (opacity < 0.0 || opacity > 1.0) {
        throw make_error(invalid_argument, "changing the window opacity",
                         "the opacity must be between 0 and 1");
    }

    const LONG_PTR style = ::GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if ((style & WS_EX_LAYERED) == 0) {
        ::SetWindowLongPtrW(hwnd, GWL_EXSTYLE, style | WS_EX_LAYERED);
    }

    const auto alpha = static_cast<BYTE>(opacity * 255.0 + 0.5);
    if (!::SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA)) {
        const auto native = static_cast<std::int32_t>(::GetLastError());
        throw make_error_text(platform_failure, "changing the window opacity",
                              "SetLayeredWindowAttributes failed" + platform_text(native));
    }

    set_opacity_value(opacity);
}

void win32_window_impl::enable(bool enabled) {
    const HWND hwnd = require_native("enabling the window");
    require_ui_thread("enabling the window");
    ::EnableWindow(hwnd, enabled ? TRUE : FALSE);
}

bool win32_window_impl::is_enabled() const {
    const auto* native = native_handle();
    return native != nullptr
            && ::IsWindowEnabled(static_cast<HWND>(const_cast<void*>(native))) != FALSE;
}

void win32_window_impl::request_attention() {
    const HWND hwnd = require_native("requesting attention");
    require_ui_thread("requesting attention");

    FLASHWINFO info{};
    info.cbSize = sizeof(info);
    info.hwnd = hwnd;
    info.dwFlags = FLASHW_ALL | FLASHW_TIMERNOFG;
    ::FlashWindowEx(&info);
}

void win32_window_impl::set_resizable(bool resizable) {
    const HWND hwnd = require_native("changing the window style");
    require_ui_thread("changing the window style");

    LONG_PTR style = ::GetWindowLongPtrW(hwnd, GWL_STYLE);
    if (resizable) {
        style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
    } else {
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    }
    ::SetWindowLongPtrW(hwnd, GWL_STYLE, style);
    ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
                           | SWP_FRAMECHANGED);

    set_resizable_value(resizable);
}

std::uint32_t win32_window_impl::dpi() const {
    const auto* native = native_handle();
    return native != nullptr
            ? window_dpi(static_cast<HWND>(const_cast<void*>(native)))
            : 96;
}

void win32_window_impl::close() {
    auto* native = native_handle();
    if (native == nullptr) return;

    if (::GetCurrentThreadId() == m_thread_id) {
        if (!::DestroyWindow(static_cast<HWND>(native))) {
            const auto error_code = static_cast<std::int32_t>(::GetLastError());
            throw make_error_text(platform_failure, "closing the window",
                                  "DestroyWindow failed" + platform_text(error_code));
        }
        return;
    }

    if (!::PostMessageW(static_cast<HWND>(native), WM_CLOSE, 0, 0)) {
        const auto error_code = static_cast<std::int32_t>(::GetLastError());
        throw make_error_text(platform_failure, "closing the window",
                              "PostMessageW(WM_CLOSE) failed" + platform_text(error_code));
    }
}

void win32_window_impl::destroy_native() noexcept {
    auto* native = native_handle();
    if (native == nullptr) return;

    if (::GetCurrentThreadId() == m_thread_id) {
        ::DestroyWindow(static_cast<HWND>(native));
        return;
    }

    // DestroyWindow only works on the owning thread; ask that thread instead.
    ::PostMessageW(static_cast<HWND>(native), WM_CLOSE, 0, 0);
}

namespace {

LRESULT CALLBACK window_proc(HWND hwnd, UINT message,
                             WPARAM wparam, LPARAM lparam) noexcept {
    win32_window_impl* window = nullptr;

    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        window = static_cast<win32_window_impl*>(create->lpCreateParams);
        if (window != nullptr) {
            window->set_native_handle(hwnd);
            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        }
    } else {
        window = reinterpret_cast<win32_window_impl*>(
                ::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (window == nullptr) return ::DefWindowProcW(hwnd, message, wparam, lparam);

    return window->dispatch(hwnd, message, wparam, lparam);
}

}  // namespace

LRESULT win32_window_impl::dispatch(HWND hwnd, UINT message,
                                    WPARAM wparam, LPARAM lparam) noexcept {
    if (message == WM_NCDESTROY) {
        mark_closed();
        set_native_handle(nullptr);
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);

        native_lost();
        return ::DefWindowProcW(hwnd, message, wparam, lparam);
    }

    // A window created visible gets messages while CreateWindowExW still runs:
    // the object is not fully constructed yet, so do not call into it.
    if (!m_created) return ::DefWindowProcW(hwnd, message, wparam, lparam);

    std::intptr_t native_result = 0;

    switch (message) {
    case WM_CLOSE: {
        const close_event event{};
        emit(event);

        if (handle_extension(message, static_cast<std::uintptr_t>(wparam),
                             static_cast<std::intptr_t>(lparam), native_result)) {
            return static_cast<LRESULT>(native_result);
        }

        if (should_close()) {
            ::DestroyWindow(hwnd);
        }
        return 0;
    }

    case WM_DESTROY:
        mark_closed();
        emit(destroy_event{});
        break;

    case WM_SHOWWINDOW:
        if (wparam != 0) {
            emit(show_event{});
        } else {
            emit(hide_event{});
        }
        break;

    case WM_SIZE: {
        state next = state::restored;
        if (wparam == SIZE_MINIMIZED) {
            next = state::minimized;
        } else if (wparam == SIZE_MAXIMIZED) {
            next = state::maximized;
        }

        if (next != m_state) {
            m_state = next;
            switch (next) {
            case state::minimized: emit(minimize_event{}); break;
            case state::maximized: emit(maximize_event{}); break;
            case state::restored: emit(restore_event{}); break;
            }
        }

        const resize_event event{static_cast<std::int32_t>(LOWORD(lparam)),
                                 static_cast<std::int32_t>(HIWORD(lparam))};
        emit(event);
        break;
    }

    case WM_MOVE: {
        const move_event event{point_x(lparam), point_y(lparam)};
        emit(event);
        break;
    }

    case WM_ACTIVATE: {
        activate_event event{};
        event.active = LOWORD(wparam) != WA_INACTIVE;
        emit(event);
        break;
    }

    case WM_SETFOCUS: {
        const focus_event event{1};
        emit(event);
        break;
    }

    case WM_KILLFOCUS: {
        const focus_event event{0};
        emit(event);
        break;
    }

    case WM_CHAR: {
        const text_event event{static_cast<std::uint32_t>(wparam)};
        emit(event);
        break;
    }

    case WM_GETMINMAXINFO: {
        if (auto* info = reinterpret_cast<MINMAXINFO*>(lparam)) {
            const window_extent minimum = min_size();
            const window_extent maximum = max_size();
            if (minimum.width > 0) info->ptMinTrackSize.x = minimum.width;
            if (minimum.height > 0) info->ptMinTrackSize.y = minimum.height;
            if (maximum.width > 0) info->ptMaxTrackSize.x = maximum.width;
            if (maximum.height > 0) info->ptMaxTrackSize.y = maximum.height;
        }
        break;
    }

    case WM_DPICHANGED: {
        dpi_event event{};
        event.dpi = HIWORD(wparam);
        if (const auto* suggested = reinterpret_cast<const RECT*>(lparam)) {
            event.x = suggested->left;
            event.y = suggested->top;
            event.width = suggested->right - suggested->left;
            event.height = suggested->bottom - suggested->top;
        }
        emit(event);
        break;
    }

    case WM_DISPLAYCHANGE: {
        display_change_event event{};
        event.bits_per_pixel = static_cast<std::uint32_t>(wparam);
        event.width = static_cast<std::int32_t>(LOWORD(lparam));
        event.height = static_cast<std::int32_t>(HIWORD(lparam));
        emit(event);
        break;
    }

    case WM_THEMECHANGED:
        emit(theme_change_event{});
        break;

    case WM_SETTINGCHANGE: {
        const settings_change_event event{static_cast<std::uint32_t>(wparam)};
        emit(event);
        break;
    }

    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_XBUTTONDBLCLK: {
        if (!m_tracking_mouse && message == WM_MOUSEMOVE) {
            TRACKMOUSEEVENT tracking{};
            tracking.cbSize = sizeof(tracking);
            tracking.dwFlags = TME_LEAVE;
            tracking.hwndTrack = hwnd;
            if (::TrackMouseEvent(&tracking)) m_tracking_mouse = true;
        }

        mouse_event event{};
        event.x = point_x(lparam);
        event.y = point_y(lparam);
        event.modifiers = mouse_modifiers(wparam);

        switch (message) {
        case WM_LBUTTONDOWN: event.button = mouse_button::left; event.action = mouse_action::down; break;
        case WM_LBUTTONUP: event.button = mouse_button::left; event.action = mouse_action::up; break;
        case WM_LBUTTONDBLCLK: event.button = mouse_button::left; event.action = mouse_action::double_click; break;
        case WM_RBUTTONDOWN: event.button = mouse_button::right; event.action = mouse_action::down; break;
        case WM_RBUTTONUP: event.button = mouse_button::right; event.action = mouse_action::up; break;
        case WM_RBUTTONDBLCLK: event.button = mouse_button::right; event.action = mouse_action::double_click; break;
        case WM_MBUTTONDOWN: event.button = mouse_button::middle; event.action = mouse_action::down; break;
        case WM_MBUTTONUP: event.button = mouse_button::middle; event.action = mouse_action::up; break;
        case WM_MBUTTONDBLCLK: event.button = mouse_button::middle; event.action = mouse_action::double_click; break;
        case WM_XBUTTONDOWN:
            event.button = (GET_XBUTTON_WPARAM(wparam) == XBUTTON1) ? mouse_button::x1
                                                                    : mouse_button::x2;
            event.action = mouse_action::down;
            break;
        case WM_XBUTTONUP:
            event.button = (GET_XBUTTON_WPARAM(wparam) == XBUTTON1) ? mouse_button::x1
                                                                    : mouse_button::x2;
            event.action = mouse_action::up;
            break;
        case WM_XBUTTONDBLCLK:
            event.button = (GET_XBUTTON_WPARAM(wparam) == XBUTTON1) ? mouse_button::x1
                                                                    : mouse_button::x2;
            event.action = mouse_action::double_click;
            break;
        default: break;
        }

        emit(event);
        break;
    }

    case WM_MOUSELEAVE:
        m_tracking_mouse = false;
        emit(mouse_leave_event{});
        break;

    case WM_MOUSEWHEEL: {
        mouse_event event{};
        event.action = mouse_action::wheel;
        event.wheel_delta = static_cast<std::int32_t>(
                static_cast<std::int16_t>(GET_WHEEL_DELTA_WPARAM(wparam)));
        event.modifiers = mouse_modifiers(wparam);

        POINT point{point_x(lparam), point_y(lparam)};
        ::ScreenToClient(hwnd, &point);
        event.x = point.x;
        event.y = point.y;

        emit(event);
        break;
    }

    case WM_MOUSEHWHEEL: {
        mouse_event event{};
        event.action = mouse_action::wheel;
        event.horizontal_wheel_delta = static_cast<std::int32_t>(
                static_cast<std::int16_t>(GET_WHEEL_DELTA_WPARAM(wparam)));
        event.modifiers = mouse_modifiers(wparam);

        POINT point{point_x(lparam), point_y(lparam)};
        ::ScreenToClient(hwnd, &point);
        event.x = point.x;
        event.y = point.y;

        emit(event);
        break;
    }

    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP: {
        key_event event{};
        event.virtual_key = static_cast<std::uint32_t>(wparam);
        event.scan_code = static_cast<std::uint32_t>((lparam >> 16) & 0xFF);
        event.repeat = static_cast<std::uint32_t>(lparam & 0xFFFF);
        if (message == WM_KEYUP || message == WM_SYSKEYUP) {
            event.action = key_action::up;
        } else {
            event.action = event.repeat > 1 ? key_action::repeat : key_action::down;
        }
        event.modifiers = current_modifiers();

        emit(event);
        break;
    }

    case WM_PAINT: {
        const paint_event event{};
        emit(event);
        break;
    }

    default:
        break;
    }

    if (handle_extension(message, static_cast<std::uintptr_t>(wparam),
                         static_cast<std::intptr_t>(lparam), native_result)) {
        return static_cast<LRESULT>(native_result);
    }

    return ::DefWindowProcW(hwnd, message, wparam, lparam);
}

std::unique_ptr<window_impl> create_window_impl(app_impl& host,
                                                const window_options& options) {
    return std::make_unique<win32_window_impl>(host, options);
}

}  // namespace helios::detail
