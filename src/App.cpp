#include "AppImpl.h"

#include <HeliosView2/App.h>

#include <utility>

namespace helios {

app::app(const app_options& options) : m_impl(detail::create_app_impl(options)) {}

app::app(app&& other) noexcept = default;

app& app::operator=(app&& other) noexcept = default;

app::~app() = default;

detail::app_impl& app::impl() const {
    if (!m_impl) {
        throw make_error(invalid_state, "using the application",
                         "the application has been moved from");
    }
    return *m_impl;
}

void app::run(std::int32_t& exit_code) {
    impl().run(exit_code);
}

void app::quit() noexcept {
    if (m_impl) m_impl->quit();
}

bool app::is_running() const noexcept {
    return m_impl && m_impl->is_running();
}

std::uint64_t app::ui_thread_id() const noexcept {
    return m_impl ? m_impl->ui_thread_id() : 0;
}

std::uint64_t app::window_count() const {
    return impl().window_count();
}

void app::close_all_windows() {
    impl().close_all_windows();
}

void app::post(std::function<void()> work, std::function<void(error)> on_failure) {
    impl().post(std::move(work), std::move(on_failure));
}

}  // namespace helios
