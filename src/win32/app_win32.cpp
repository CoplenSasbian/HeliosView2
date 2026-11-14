#include "../AppImpl.h"
#include "../WindowImpl.h"

#include <HeliosView2/App.h>

#ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#    define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>

#include <algorithm>
#include <cstdio>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace helios::detail {

namespace {

constexpr UINT kTaskMessage = WM_APP + 1;

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

std::string hresult_text(HRESULT result) {
    wchar_t* buffer = nullptr;
    const DWORD length = ::FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                    | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, static_cast<DWORD>(result), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
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

    char digits[16]{};
    std::snprintf(digits, sizeof(digits), "0x%08lX", static_cast<unsigned long>(result));
    std::string suffix = " (error ";
    suffix += digits;
    if (!text.empty()) {
        suffix += ": ";
        suffix += text;
    }
    suffix += ')';
    return suffix;
}

}  // namespace

class win32_app_impl final : public app_impl {
public:
    explicit win32_app_impl(const app_options& options)
        : m_thread_id(::GetCurrentThreadId()),
          m_quit_on_last_window_closed(options.quit_on_last_window_closed) {
        set_dpi_awareness();

        const HRESULT result = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(result)) {
            throw make_error_text(platform_failure, "creating the application",
                                  "CoInitializeEx failed" + hresult_text(result));
        }
        m_platform_initialized = true;

        set_app_user_model_id(options.id);

        MSG message{};
        ::PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE);
    }

    ~win32_app_impl() override {
        // Detach first: destroying a native window notifies the app, and the
        // app is already going away.
        std::vector<window_impl*> windows;
        {
            std::lock_guard lock(m_windows_mutex);
            windows.swap(m_windows);
        }

        for (window_impl* window : windows) {
            window->detach_host();
            window->destroy_native();
        }

        discard_tasks();
        if (m_platform_initialized) ::CoUninitialize();
    }

    void run(std::int32_t& exit_code) override {
        if (::GetCurrentThreadId() != m_thread_id) {
            throw make_error(
                    wrong_thread, "running the message loop",
                    "run must be called from the thread that created the app");
        }
        if (m_started.exchange(true)) {
            throw make_error(invalid_state, "running the message loop",
                             "the message loop is already running");
        }

        bool expected = false;
        if (!m_running.compare_exchange_strong(expected, true)) {
            m_stopped.store(true, std::memory_order_release);
            throw make_error(invalid_state, "running the message loop",
                             "the message loop has already finished");
        }

        exit_code = 0;

        if (m_quit_requested.exchange(false)) {
            discard_tasks();
            finish();
            return;
        }

        drain_tasks();

        MSG message{};
        for (;;) {
            const BOOL result = ::GetMessageW(&message, nullptr, 0, 0);
            if (result == -1) {
                const auto native = static_cast<std::int32_t>(::GetLastError());
                discard_tasks();
                finish();
                throw make_error_text(platform_failure, "pumping the message loop",
                                      "GetMessageW failed (error "
                                              + std::to_string(native) + ')');
            }
            if (result == 0) {
                exit_code = static_cast<std::int32_t>(message.wParam);
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
        finish();
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

    std::uint64_t window_count() const override {
        std::lock_guard lock(m_windows_mutex);
        return static_cast<std::uint64_t>(m_windows.size());
    }

    void close_all_windows() override {
        // Work on a snapshot: a window destroyed by close() unregisters itself
        // from the app, which would invalidate an iterator over the registry.
        std::vector<window_impl*> windows;
        {
            std::lock_guard lock(m_windows_mutex);
            windows.assign(m_windows.begin(), m_windows.end());
        }

        for (window_impl* window : windows) {
            try {
                window->close();
            } catch (const error&) {
            }
        }
    }

    void post(std::function<void()> work,
              std::function<void(error)> on_failure) override {
        if (!work) {
            throw make_error(invalid_argument, "posting a task", "the task is empty");
        }
        if (m_stopped.load(std::memory_order_acquire)) {
            throw make_error(invalid_state, "posting a task", "the app has already stopped");
        }

        // A task runs later on the UI thread, so an exception has nobody to
        // return to: catch it here and hand it to the caller's handler.
        std::function<void()> item =
                [work = std::move(work), on_failure = std::move(on_failure)]() mutable {
                    try {
                        work();
                    } catch (const std::exception& failure) {
                        if (on_failure) {
                            on_failure(make_error(unhandled_exception,
                                                  "running a posted task", "{}",
                                                  failure.what()));
                        }
                    } catch (...) {
                        if (on_failure) {
                            on_failure(make_error(
                                    unhandled_exception, "running a posted task",
                                    "the task threw a value that is not an exception"));
                        }
                    }
                };

        try {
            std::lock_guard lock(m_tasks_mutex);
            m_tasks.push_back(std::move(item));
        } catch (...) {
            throw make_error(out_of_memory, "posting a task",
                             "the task queue could not grow");
        }

        ::PostThreadMessageW(m_thread_id, kTaskMessage, 0, 0);
    }

    void add_window(window_impl& window) override {
        std::lock_guard lock(m_windows_mutex);
        try {
            m_windows.push_back(&window);
        } catch (...) {
            throw make_error(out_of_memory, "creating a window",
                             "the window registry could not grow");
        }
    }

    void remove_window(window_impl& window) noexcept override {
        std::lock_guard lock(m_windows_mutex);
        const auto it = std::find(m_windows.begin(), m_windows.end(), &window);
        if (it != m_windows.end()) m_windows.erase(it);
    }

    void window_destroyed(window_impl& window) noexcept override {
        bool empty = false;
        {
            std::lock_guard lock(m_windows_mutex);
            const auto it = std::find(m_windows.begin(), m_windows.end(), &window);
            if (it != m_windows.end()) m_windows.erase(it);
            empty = m_windows.empty();
        }

        if (empty && m_quit_on_last_window_closed) quit();
    }

private:
    void finish() noexcept {
        m_running.store(false, std::memory_order_release);
        m_stopped.store(true, std::memory_order_release);
    }

    void drain_tasks() noexcept {
        for (;;) {
            std::function<void()> item;
            {
                std::lock_guard lock(m_tasks_mutex);
                if (m_tasks.empty()) break;
                item = std::move(m_tasks.front());
                m_tasks.pop_front();
            }
            if (item) {
                try {
                    item();
                } catch (...) {
                }
            }
        }
    }

    void discard_tasks() noexcept {
        std::deque<std::function<void()>> pending;
        {
            std::lock_guard lock(m_tasks_mutex);
            pending.swap(m_tasks);
        }
    }

    DWORD m_thread_id{};
    bool m_quit_on_last_window_closed{true};
    bool m_platform_initialized{false};
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_started{false};
    std::atomic<bool> m_stopped{false};
    std::atomic<bool> m_quit_requested{false};
    std::mutex m_tasks_mutex;
    std::deque<std::function<void()>> m_tasks;
    mutable std::mutex m_windows_mutex;
    std::vector<window_impl*> m_windows;
};

std::unique_ptr<app_impl> create_app_impl(const app_options& options) {
    try {
        return std::make_unique<win32_app_impl>(options);
    } catch (const error&) {
        throw;
    } catch (const std::bad_alloc&) {
        throw make_error(out_of_memory, "creating the application",
                         "the app object could not be allocated");
    } catch (const std::exception& failure) {
        throw make_error(out_of_memory, "creating the application", "{}",
                         failure.what());
    }
}

}  // namespace helios::detail
