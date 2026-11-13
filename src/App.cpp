// The public handles: every method is a thin forward to the backend, which keeps
// the headers free of platform details and the calls non-virtual.

#include <HeliosView2/App.h>
#include <HeliosView2/Window.h>

#include "AppBackend.h"

namespace helios
{
    app::app(std::unique_ptr<detail::app_impl> impl) noexcept : m_impl(std::move(impl))
    {
    }

    app::~app() = default;

    void app::run(std::int32_t& exit_code)
    {
        m_impl->run(exit_code);
    }

    void app::quit() noexcept
    {
        m_impl->quit();
    }

    bool app::is_running() const noexcept
    {
        return m_impl->is_running();
    }

    std::uint64_t app::ui_thread_id() const noexcept
    {
        return m_impl->ui_thread_id();
    }

    std::shared_ptr<base_window> app::create_window(const window_options& options)
    {
        return m_impl->create_window(options);
    }

    std::uint64_t app::window_count() const
    {
        return m_impl->window_count();
    }

    void app::close_all_windows()
    {
        m_impl->close_all_windows();
    }

    void app::post(std::function<void()> work, std::function<void(error)> on_failure)
    {
        m_impl->post(std::move(work), std::move(on_failure));
    }

    base_window::base_window(std::unique_ptr<detail::window_impl> impl) noexcept
        : m_impl(std::move(impl))
    {
    }

    base_window::~base_window() = default;

    void base_window::show()
    {
        m_impl->show();
    }

    void base_window::hide()
    {
        m_impl->hide();
    }

    void base_window::close()
    {
        m_impl->close();
    }

    void base_window::set_title(std::string_view title)
    {
        m_impl->set_title(title);
    }

    void base_window::set_should_close(bool should_close)
    {
        m_impl->m_should_close = should_close;
    }

    bool base_window::should_close() const
    {
        return m_impl->m_should_close;
    }

    bool base_window::is_closed() const
    {
        return m_impl->is_closed();
    }

    void* base_window::native_handle() const
    {
        return m_impl->native_handle();
    }
} // namespace helios
