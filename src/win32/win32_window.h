#pragma once

#include <HeliosView2/Window.h>

#ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#    define NOMINMAX
#endif
#include <windows.h>

#include <atomic>
#include <cstdint>
#include <string>

namespace helios {

class win32_window;

class win32_window_host {
public:
    virtual void window_destroyed(win32_window& window) noexcept = 0;

protected:
    ~win32_window_host() = default;
};

class win32_window final : public base_window {
public:
    static abi::status create(win32_window_host& host,
                              const abi::window_options& options,
                              win32_window** out) noexcept;

    ~win32_window() = default;

    std::uint32_t add_ref() noexcept override;
    std::uint32_t release() noexcept override;
    abi::status query_interface(const abi::guid& id, void** out) noexcept override;

    abi::status show() noexcept override;
    abi::status hide() noexcept override;
    abi::status close() noexcept override;
    abi::status set_title(const char* utf8, std::size_t size) noexcept override;

    [[nodiscard]] bool is_closed() const noexcept override;
    [[nodiscard]] void* native_handle() const noexcept override;

    [[nodiscard]] HWND hwnd() const noexcept { return m_hwnd; }

    static LRESULT CALLBACK window_proc(HWND hwnd, UINT message,
                                        WPARAM wparam, LPARAM lparam) noexcept;

private:
    win32_window(win32_window_host& host, const abi::window_options& options);

    LRESULT handle_message(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) noexcept;

    win32_window_host* m_host;
    HWND m_hwnd{nullptr};
    DWORD m_thread_id{};
    std::atomic<std::uint32_t> m_refs{1};
    std::atomic<bool> m_closed{false};
    std::string m_title;
};

}  // namespace helios
