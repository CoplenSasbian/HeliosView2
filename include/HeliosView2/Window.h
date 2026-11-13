#pragma once

// Public window interface.
//
// Like app: a handle over a pimpl that lives in the DLL, so the header stays small
// and nothing platform specific leaks out.

#include <HeliosView2/App.h>
#include <HeliosView2/HeliosView2_export.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace helios {

namespace detail {
class window_impl;
}  // namespace detail

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
};

class HELIOSVIEW2_EXPORT base_window {
public:
    base_window(const base_window&) = delete;
    base_window& operator=(const base_window&) = delete;
    ~base_window();

    void show();
    void hide();
    void close();
    void set_title(std::string_view title);
    void set_should_close(bool should_close);
    bool should_close() const;
    [[nodiscard]] bool is_closed() const;
    [[nodiscard]] void* native_handle() const;

private:
    friend struct detail::app_access;

    explicit base_window(std::unique_ptr<detail::window_impl> impl) noexcept;

    std::unique_ptr<detail::window_impl> m_impl;
};

/// Convenience for `application.create_window(options)`; throws on failure.
[[nodiscard]] inline std::shared_ptr<base_window> create_window(
        app& target, const window_options& options) {
    return target.create_window(options);
}

}  // namespace helios
