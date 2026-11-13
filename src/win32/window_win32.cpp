#include "win32_window.h"

#include <mutex>
#include <string>
#include <utility>

namespace helios::detail {

namespace {

constexpr wchar_t kWindowClass[] = L"HeliosView2.Window";

std::once_flag g_register_once;
ATOM g_class_atom = 0;

bool ensure_window_class() noexcept {
    std::call_once(g_register_once, [] {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = &win32_window_impl::window_proc;
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

}  // namespace

win32_window_impl::win32_window_impl(app_impl& owner, const window_options& options)
    : m_owner(&owner), m_thread_id(::GetCurrentThreadId()), m_title(options.title) {}

win32_window_impl::~win32_window_impl() {
    if (m_hwnd != nullptr) ::DestroyWindow(m_hwnd);
}

std::unique_ptr<win32_window_impl> win32_window_impl::create(
        app_impl& owner, const window_options& options) {
    if (!ensure_window_class()) {
        const auto native = static_cast<std::int32_t>(::GetLastError());
        throw make_error_text(
                platform_failure, "creating the window",
                "RegisterClassExW failed" + platform_text(native));
    }

    auto window = std::unique_ptr<win32_window_impl>(new win32_window_impl(owner, options));

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
                                        window.get());
    if (hwnd == nullptr) {
        const auto native = static_cast<std::int32_t>(::GetLastError());
        throw make_error_text(
                platform_failure, "creating the window",
                "CreateWindowExW failed for " + std::to_string(width) + "x"
                        + std::to_string(height) + platform_text(native));
    }

    return window;
}

void win32_window_impl::show() {
    if (m_hwnd == nullptr) {
        throw make_error(invalid_state, "showing the window",
                                          "the native window is gone");
    }
    if (::GetCurrentThreadId() != m_thread_id) {
        throw make_error(wrong_thread, "showing the window",
                                          "a window can only be shown on the UI thread");
    }

    ::ShowWindow(m_hwnd, SW_SHOW);
    ::UpdateWindow(m_hwnd);
    return;
}

void win32_window_impl::hide() {
    if (m_hwnd == nullptr) {
        throw make_error(invalid_state, "hiding the window",
                                          "the native window is gone");
    }
    if (::GetCurrentThreadId() != m_thread_id) {
        throw make_error(wrong_thread, "hiding the window",
                                          "a window can only be hidden on the UI thread");
    }

    ::ShowWindow(m_hwnd, SW_HIDE);
}

void win32_window_impl::close() {
    if (m_hwnd == nullptr) return;

    if (::GetCurrentThreadId() == m_thread_id) {
        if (!::DestroyWindow(m_hwnd)) {
            const auto native = static_cast<std::int32_t>(::GetLastError());
            throw make_error_text(
                    platform_failure, "closing the window",
                    "DestroyWindow failed" + platform_text(native));
        }
        return;
    }

    if (!::PostMessageW(m_hwnd, WM_CLOSE, 0, 0)) {
        const auto native = static_cast<std::int32_t>(::GetLastError());
        throw make_error_text(
                platform_failure, "closing the window",
                "PostMessageW(WM_CLOSE) failed" + platform_text(native));
    }
    return;
}

void win32_window_impl::set_title(std::string_view title) {
    if (m_hwnd == nullptr) {
        throw make_error(invalid_state, "setting the window title",
                                          "the native window is gone");
    }
    if (::GetCurrentThreadId() != m_thread_id) {
        throw make_error(wrong_thread, "setting the window title",
                                          "a window can only be retitled on the UI thread");
    }

    const std::wstring wide = to_wide(title);
    if (!::SetWindowTextW(m_hwnd, wide.c_str())) {
        const auto native = static_cast<std::int32_t>(::GetLastError());
        throw make_error_text(
                platform_failure, "setting the window title",
                "SetWindowTextW failed" + platform_text(native));
    }

    m_title.assign(title);
    return;
}

bool win32_window_impl::is_closed() const {
    return m_closed.load(std::memory_order_acquire);
}

void* win32_window_impl::native_handle() const {
    return m_hwnd;
}

void win32_window_impl::detach() noexcept {
    m_owner = nullptr;
}

LRESULT CALLBACK win32_window_impl::window_proc(HWND hwnd, UINT message,
                                                WPARAM wparam, LPARAM lparam) noexcept {
    win32_window_impl* self = nullptr;

    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        self = static_cast<win32_window_impl*>(create->lpCreateParams);
        if (self != nullptr) {
            self->m_hwnd = hwnd;
            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
    } else {
        self = reinterpret_cast<win32_window_impl*>(
                ::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self == nullptr) return ::DefWindowProcW(hwnd, message, wparam, lparam);

    // Keep the public handle (and with it this object) alive for the duration of
    // the message: the app may drop its reference from window_destroyed().
    const std::shared_ptr<base_window> keep_alive = self->lock_handle();
    return self->handle_message(hwnd, message, wparam, lparam);
}

LRESULT win32_window_impl::handle_message(HWND hwnd, UINT message,
                                          WPARAM wparam, LPARAM lparam) noexcept {
    switch (message) {
    case WM_CLOSE:
        if (m_hwnd != nullptr && m_should_close) ::DestroyWindow(m_hwnd);
        return 0;

    case WM_DESTROY:
        m_closed.store(true, std::memory_order_release);
        return 0;

    case WM_NCDESTROY: {
        m_closed.store(true, std::memory_order_release);
        m_hwnd = nullptr;
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);

        app_impl* owner = m_owner;
        m_owner = nullptr;
        if (owner != nullptr) owner->window_destroyed(*this);
        return ::DefWindowProcW(hwnd, message, wparam, lparam);
    }

    default:
        return ::DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

}  // namespace helios::detail
