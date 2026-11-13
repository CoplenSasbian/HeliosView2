#pragma once

#include "../AppBackend.h"

#include <HeliosView2/Window.h>

#ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#    define NOMINMAX
#endif
#include <windows.h>

#include <atomic>
#include <memory>
#include <string>
#include <string_view>

namespace helios::detail {

class win32_window_impl final : public window_impl {
public:
    ~win32_window_impl() override;

    void show() override;
    void hide() override;
    void close() override;
    void set_title(std::string_view title) override;

    [[nodiscard]] bool is_closed() const override;
    [[nodiscard]] void* native_handle() const override;

    void detach() noexcept override;

    /// Creates the native window; `owner` is notified when it is destroyed.
    [[nodiscard]] static std::unique_ptr<win32_window_impl> create(
            app_impl& owner, const window_options& options);

    static LRESULT CALLBACK window_proc(HWND hwnd, UINT message,
                                        WPARAM wparam, LPARAM lparam) noexcept;

private:
    explicit win32_window_impl(app_impl& owner, const window_options& options);

    LRESULT handle_message(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) noexcept;

    app_impl* m_owner;
    HWND m_hwnd{nullptr};
    DWORD m_thread_id{};
    std::atomic<bool> m_closed{false};
    std::string m_title;
};

}  // namespace helios::detail
