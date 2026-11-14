#include "../WindowImpl.h"

#include <HeliosView2/Error.h>
#include <HeliosView2/Window.h>

namespace helios::detail {

namespace {

[[noreturn]] void no_backend(const char* context) {
    throw make_error(unsupported, context, "this build has no windowing backend");
}

class stub_window_impl final : public window_impl {
public:
    using window_impl::window_impl;

    void show() override { no_backend("showing the window"); }
    void hide() override { no_backend("hiding the window"); }
    [[nodiscard]] bool is_visible() const override { return false; }

    void set_title(std::string_view) override { no_backend("setting the window title"); }
    [[nodiscard]] std::string title() const override { return {}; }

    void set_size(std::int32_t, std::int32_t) override { no_backend("resizing the window"); }
    [[nodiscard]] window_extent size() const override { return {}; }
    [[nodiscard]] window_extent client_size() const override { return {}; }

    void set_position(std::int32_t, std::int32_t) override { no_backend("moving the window"); }
    [[nodiscard]] std::pair<std::int32_t, std::int32_t> position() const override {
        return {};
    }
    void center() override { no_backend("centering the window"); }

    void minimize() override { no_backend("minimizing the window"); }
    void maximize() override { no_backend("maximizing the window"); }
    void restore() override { no_backend("restoring the window"); }
    [[nodiscard]] bool is_minimized() const override { return false; }
    [[nodiscard]] bool is_maximized() const override { return false; }

    void focus() override { no_backend("focusing the window"); }
    [[nodiscard]] bool is_focused() const override { return false; }
    void activate() override { no_backend("activating the window"); }

    void set_always_on_top(bool) override { no_backend("changing the window z-order"); }
    void set_opacity(double) override { no_backend("changing the window opacity"); }
    void enable(bool) override { no_backend("enabling the window"); }
    [[nodiscard]] bool is_enabled() const override { return false; }
    void request_attention() override { no_backend("requesting attention"); }
    void set_resizable(bool) override { no_backend("changing the window style"); }

    [[nodiscard]] std::uint32_t dpi() const override { return 96; }

    void close() override { no_backend("closing the window"); }
    void destroy_native() noexcept override {}
};

}  // namespace

std::unique_ptr<window_impl> create_window_impl(app_impl& host,
                                                const window_options& options) {
    return std::make_unique<stub_window_impl>(host, options);
}

}  // namespace helios::detail
