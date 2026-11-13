#include "../abi/app_factory.h"
#include "win32_window.h"
#include <HeliosView2/Task.h>

#ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#    define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace helios {

namespace {

constexpr UINT kTaskMessage = WM_APP + 1;

bool valid_field(const char* data, std::size_t size) noexcept {
    return size == 0 || data != nullptr;
}

std::string copy_field(const char* data, std::size_t size) {
    if (data == nullptr || size == 0) return {};
    return std::string(data, size);
}

std::wstring to_wide(const std::string& value) {
    if (value.empty()) return {};
    const int length = ::MultiByteToWideChar(CP_UTF8, 0, value.data(),
                                             static_cast<int>(value.size()), nullptr, 0);
    if (length <= 0) return {};
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                          result.data(), length);
    return result;
}

template <class Function>
Function load_proc(HMODULE module, const char* name) noexcept {
    return reinterpret_cast<Function>(reinterpret_cast<void*>(::GetProcAddress(module, name)));
}

void set_dpi_awareness() noexcept {
    using set_context_fn = BOOL(WINAPI*)(HANDLE);
    const HMODULE user32 = ::GetModuleHandleW(L"user32.dll");
    if (user32 == nullptr) return;

    const auto set_context = load_proc<set_context_fn>(user32, "SetProcessDpiAwarenessContext");
    if (set_context != nullptr) {
        // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 == (HANDLE)-4
        set_context(reinterpret_cast<HANDLE>(static_cast<std::intptr_t>(-4)));
    }
}

void set_app_user_model_id(const std::string& id) {
    if (id.empty()) return;

    const std::wstring wide = to_wide(id);
    if (wide.empty()) return;

    using set_aumid_fn = HRESULT(WINAPI*)(PCWSTR);
    HMODULE shell = ::GetModuleHandleW(L"shell32.dll");
    if (shell == nullptr) shell = ::LoadLibraryW(L"shell32.dll");
    if (shell == nullptr) return;

    const auto set_aumid = load_proc<set_aumid_fn>(
            shell, "SetCurrentProcessExplicitAppUserModelID");
    if (set_aumid != nullptr) set_aumid(wide.c_str());
}

}  // namespace

class win32_app final : public app, private win32_window_host {
public:
    explicit win32_app(const app_options& options)
        : m_thread_id(::GetCurrentThreadId()) {
        if (!valid_field(options.id, options.id_size)
            || !valid_field(options.name, options.name_size)
            || !valid_field(options.version, options.version_size)) {
            m_init_status = abi::invalid_argument;
            return;
        }

        m_id = copy_field(options.id, options.id_size);
        m_name = copy_field(options.name, options.name_size);
        m_version = copy_field(options.version, options.version_size);
        m_quit_on_last_window_closed = options.quit_on_last_window_closed != 0;

        set_dpi_awareness();

        const HRESULT result = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(result)) {
            m_init_status = abi::unsupported;
            return;
        }
        m_com_initialized = true;

        set_app_user_model_id(m_id);

        MSG message{};
        ::PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE);
    }

    ~win32_app() {
        close_all_windows();
        discard_tasks();
        if (m_com_initialized) ::CoUninitialize();
    }

    abi::status init_status() const noexcept { return m_init_status; }

    std::uint32_t add_ref() noexcept override { return ++m_refs; }

    std::uint32_t release() noexcept override {
        const auto remaining = --m_refs;
        if (remaining == 0) delete this;
        return remaining;
    }

    abi::status query_interface(const abi::guid& id, void** out) noexcept override {
        if (out == nullptr) return abi::invalid_argument;
        *out = nullptr;

        if (id == app::iid) {
            *out = static_cast<app*>(this);
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

    abi::status run(std::int32_t* exit_code) noexcept override {
        if (::GetCurrentThreadId() != m_thread_id) return abi::wrong_thread;
        if (m_started.exchange(true)) return abi::invalid_argument;

        bool expected = false;
        if (!m_running.compare_exchange_strong(expected, true)) {
            m_stopped.store(true, std::memory_order_release);
            return abi::invalid_argument;
        }

        if (exit_code != nullptr) *exit_code = 0;

        if (m_quit_requested.exchange(false)) {
            discard_tasks();
            m_running.store(false, std::memory_order_release);
            m_stopped.store(true, std::memory_order_release);
            return abi::ok;
        }

        drain_tasks();

        MSG message{};
        for (;;) {
            const BOOL result = ::GetMessageW(&message, nullptr, 0, 0);
            if (result == -1) {
                discard_tasks();
                m_running.store(false, std::memory_order_release);
                m_stopped.store(true, std::memory_order_release);
                return abi::unsupported;
            }
            if (result == 0) {
                if (exit_code != nullptr) *exit_code = static_cast<std::int32_t>(message.wParam);
                break;
            }
            if (message.message == kTaskMessage) {
                drain_tasks();
                continue;
            }
            ::TranslateMessage(&message);
            ::DispatchMessageW(&message);
        }

        discard_tasks();
        m_running.store(false, std::memory_order_release);
        m_stopped.store(true, std::memory_order_release);
        return abi::ok;
    }

    void quit() noexcept override {
        m_quit_requested.store(true, std::memory_order_relaxed);
        if (m_running.load(std::memory_order_acquire)) {
            ::PostThreadMessageW(m_thread_id, WM_QUIT, 0, 0);
        }
    }

    bool is_running() const noexcept override {
        return m_running.load(std::memory_order_acquire);
    }

    std::uint64_t ui_thread_id() const noexcept override {
        return static_cast<std::uint64_t>(m_thread_id);
    }

    abi::status create_window(const abi::window_options* options, base_window** out) noexcept override {
        if (out == nullptr) return abi::invalid_argument;
        *out = nullptr;
        if (options == nullptr) return abi::invalid_argument;
        if (::GetCurrentThreadId() != m_thread_id) return abi::wrong_thread;

        win32_window* raw = nullptr;
        const abi::status status = win32_window::create(*this, *options, &raw);
        if (abi::failed(status)) return status;

        raw->add_ref();  // caller's reference
        abi::ptr<win32_window> owned(raw);  // app's owning reference

        try {
            std::lock_guard<std::mutex> lock(m_windows_mutex);
            m_windows.push_back(std::move(owned));
        } catch (...) {
            raw->release();
            return abi::out_of_memory;
        }

        *out = static_cast<base_window*>(raw);
        return abi::ok;
    }

    std::uint64_t window_count() const noexcept override {
        std::lock_guard<std::mutex> lock(m_windows_mutex);
        return static_cast<std::uint64_t>(m_windows.size());
    }

    abi::status post(abi::task* work) noexcept override {
        if (work == nullptr) return abi::invalid_argument;
        if (m_stopped.load(std::memory_order_acquire)) return abi::invalid_argument;

        work->add_ref();
        try {
            std::lock_guard<std::mutex> lock(m_tasks_mutex);
            m_tasks.emplace_back(work);
        } catch (...) {
            work->release();
            return abi::out_of_memory;
        }

        ::PostThreadMessageW(m_thread_id, kTaskMessage, 0, 0);
        return abi::ok;
    }

private:
    void window_destroyed(win32_window& window) noexcept override {
        abi::ptr<win32_window> removed;
        bool empty = false;

        {
            std::lock_guard<std::mutex> lock(m_windows_mutex);
            const auto it = std::find_if(
                    m_windows.begin(), m_windows.end(),
                    [&](const abi::ptr<win32_window>& item) {
                        return item.get() == &window;
                    });
            if (it != m_windows.end()) {
                removed = std::move(*it);
                m_windows.erase(it);
            }
            empty = m_windows.empty();
        }

        if (empty && m_quit_on_last_window_closed) quit();
    }

    void close_all_windows() noexcept {
        for (;;) {
            HWND hwnd = nullptr;
            {
                std::lock_guard<std::mutex> lock(m_windows_mutex);
                if (m_windows.empty()) break;
                hwnd = m_windows.back()->hwnd();
            }

            if (hwnd != nullptr && ::DestroyWindow(hwnd)) continue;

            abi::ptr<win32_window> stale;
            {
                std::lock_guard<std::mutex> lock(m_windows_mutex);
                if (!m_windows.empty()) {
                    stale = std::move(m_windows.back());
                    m_windows.pop_back();
                }
            }
        }
    }

    void drain_tasks() noexcept {
        for (;;) {
            abi::ptr<abi::task> item;
            {
                std::lock_guard<std::mutex> lock(m_tasks_mutex);
                if (m_tasks.empty()) break;
                item = std::move(m_tasks.front());
                m_tasks.pop_front();
            }
            if (item != nullptr) item->run();
        }
    }

    void discard_tasks() noexcept {
        std::deque<abi::ptr<abi::task>> pending;
        {
            std::lock_guard<std::mutex> lock(m_tasks_mutex);
            pending.swap(m_tasks);
        }
    }

    std::atomic<std::uint32_t> m_refs{1};
    DWORD m_thread_id{};
    abi::status m_init_status{abi::ok};
    bool m_com_initialized{false};
    bool m_quit_on_last_window_closed{true};
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_started{false};
    std::atomic<bool> m_stopped{false};
    std::atomic<bool> m_quit_requested{false};
    std::mutex m_tasks_mutex;
    std::deque<abi::ptr<abi::task>> m_tasks;
    mutable std::mutex m_windows_mutex;
    std::vector<abi::ptr<win32_window>> m_windows;
    std::string m_id;
    std::string m_name;
    std::string m_version;
};

}  // namespace helios

namespace helios::detail {

abi::status create_platform_app(const app_options& options, app** out) noexcept {
    if (out == nullptr) return abi::invalid_argument;
    *out = nullptr;

    win32_app* instance = nullptr;
    try {
        instance = new win32_app(options);
    } catch (...) {
        return abi::out_of_memory;
    }

    if (abi::failed(instance->init_status())) {
        const abi::status status = instance->init_status();
        instance->release();
        return status;
    }

    *out = static_cast<app*>(instance);
    return abi::ok;
}

}  // namespace helios::detail
