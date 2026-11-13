#include "win32_window.h"

#include <limits>
#include <mutex>
#include <new>
#include <string>

namespace helios {

namespace {

constexpr wchar_t kWindowClass[] = L"HeliosView2.Window";

std::once_flag g_register_once;
ATOM g_class_atom = 0;

bool ensure_window_class() noexcept {
    std::call_once(g_register_once, [] {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = &win32_window::window_proc;
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

std::wstring to_wide(const char* data, std::size_t size) {
    if (data == nullptr || size == 0) return {};

    const int length = ::MultiByteToWideChar(CP_UTF8, 0, data,
                                             static_cast<int>(size), nullptr, 0);
    if (length <= 0) return {};

    std::wstring result(static_cast<std::size_t>(length), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, data, static_cast<int>(size),
                          result.data(), length);
    return result;
}

}  // namespace

win32_window::win32_window(win32_window_host& host, const abi::window_options& options)
    : m_host(&host), m_thread_id(::GetCurrentThreadId()) {
    if (options.title != nullptr && options.title_size != 0) {
        m_title.assign(options.title, options.title_size);
    }
}

abi::status win32_window::create(win32_window_host& host,
                                 const abi::window_options& options,
                                 win32_window** out) noexcept {
    if (out == nullptr) return abi::invalid_argument;
    *out = nullptr;

    if (options.title_size != 0 && options.title == nullptr) return abi::invalid_argument;
    if (!ensure_window_class()) return abi::unsupported;

    win32_window* window = nullptr;
    try {
        window = new win32_window(host, options);
    } catch (...) {
        return abi::out_of_memory;
    }

    const std::wstring title = to_wide(options.title, options.title_size);

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    if (options.resizable != 0) style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
    if (options.visible != 0) style |= WS_VISIBLE;

    const int x = options.x == std::numeric_limits<std::int32_t>::min()
                        ? CW_USEDEFAULT
                        : options.x;
    const int y = options.y == std::numeric_limits<std::int32_t>::min()
                        ? CW_USEDEFAULT
                        : options.y;
    const int width = options.width > 0 ? options.width : 800;
    const int height = options.height > 0 ? options.height : 600;

    HWND hwnd = ::CreateWindowExW(0, kWindowClass,
                                  title.empty() ? L"HeliosView2" : title.c_str(),
                                  style, x, y, width, height,
                                  nullptr, nullptr, ::GetModuleHandleW(nullptr),
                                  window);
    if (hwnd == nullptr) {
        window->release();
        return abi::unsupported;
    }

    *out = window;
    return abi::ok;
}

std::uint32_t win32_window::add_ref() noexcept {
    return ++m_refs;
}

std::uint32_t win32_window::release() noexcept {
    const auto remaining = --m_refs;
    if (remaining == 0) delete this;
    return remaining;
}

abi::status win32_window::query_interface(const abi::guid& id, void** out) noexcept {
    if (out == nullptr) return abi::invalid_argument;
    *out = nullptr;

    if (id == base_window::iid) {
        *out = static_cast<base_window*>(this);
        add_ref();
        return abi::ok;
    }
    if (id == abi::unknown_iid) {
        *out = static_cast<abi::unknown*>(this);
        add_ref();
        return abi::ok;
    }
    return abi::no_interface;
}

abi::status win32_window::show() noexcept {
    if (m_hwnd == nullptr) return abi::invalid_argument;
    if (::GetCurrentThreadId() != m_thread_id) return abi::wrong_thread;

    ::ShowWindow(m_hwnd, SW_SHOW);
    ::UpdateWindow(m_hwnd);
    return abi::ok;
}

abi::status win32_window::hide() noexcept {
    if (m_hwnd == nullptr) return abi::invalid_argument;
    if (::GetCurrentThreadId() != m_thread_id) return abi::wrong_thread;

    ::ShowWindow(m_hwnd, SW_HIDE);
    return abi::ok;
}

abi::status win32_window::close() noexcept {
    if (m_hwnd == nullptr) return abi::ok;

    if (::GetCurrentThreadId() == m_thread_id) {
        if (!::DestroyWindow(m_hwnd)) return abi::unsupported;
        return abi::ok;
    }

    if (!::PostMessageW(m_hwnd, WM_CLOSE, 0, 0)) return abi::unsupported;
    return abi::ok;
}

abi::status win32_window::set_title(const char* utf8, std::size_t size) noexcept {
    if (size != 0 && utf8 == nullptr) return abi::invalid_argument;
    if (m_hwnd == nullptr) return abi::invalid_argument;
    if (::GetCurrentThreadId() != m_thread_id) return abi::wrong_thread;

    const std::wstring title = to_wide(utf8, size);
    if (!::SetWindowTextW(m_hwnd, title.c_str())) return abi::unsupported;

    if (utf8 != nullptr && size != 0) {
        m_title.assign(utf8, size);
    } else {
        m_title.clear();
    }
    return abi::ok;
}

bool win32_window::is_closed() const noexcept {
    return m_closed.load(std::memory_order_acquire);
}

void* win32_window::native_handle() const noexcept {
    return m_hwnd;
}

LRESULT CALLBACK win32_window::window_proc(HWND hwnd, UINT message,
                                           WPARAM wparam, LPARAM lparam) noexcept {
    win32_window* self = nullptr;

    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        self = static_cast<win32_window*>(create->lpCreateParams);
        if (self != nullptr) {
            self->m_hwnd = hwnd;
            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                                reinterpret_cast<LONG_PTR>(self));
        }
    } else {
        self = reinterpret_cast<win32_window*>(
                ::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self == nullptr) return ::DefWindowProcW(hwnd, message, wparam, lparam);

    // Keep the object alive for the duration of this message: the app may drop
    // its owning reference from window_destroyed().
    self->add_ref();
    const LRESULT result = self->handle_message(hwnd, message, wparam, lparam);
    self->release();
    return result;
}

LRESULT win32_window::handle_message(HWND hwnd, UINT message,
                                     WPARAM wparam, LPARAM lparam) noexcept {
    switch (message) {
    case WM_CLOSE:
        if (m_hwnd != nullptr) ::DestroyWindow(m_hwnd);
        return 0;

    case WM_DESTROY:
        m_closed.store(true, std::memory_order_release);
        return 0;

    case WM_NCDESTROY:
        m_closed.store(true, std::memory_order_release);
        m_hwnd = nullptr;
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        if (m_host != nullptr) m_host->window_destroyed(*this);
        return ::DefWindowProcW(hwnd, message, wparam, lparam);

    default:
        return ::DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

}  // namespace helios
