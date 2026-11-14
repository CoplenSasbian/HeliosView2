#include "WindowImpl.h"

#include "AppImpl.h"

namespace helios::detail {

namespace {

void require_extent(window_extent size) {
    if (size.width < 0 || size.height < 0) {
        throw make_error(invalid_argument, "setting the window size limits",
                         "the size limits must not be negative");
    }
}

}  // namespace

window_impl::window_impl(app_impl& host, const window_options& options)
    : m_host(&host),
      m_resizable(options.resizable),
      m_always_on_top(options.always_on_top) {
    host.add_window(*this);
}

window_impl::~window_impl() {
    if (m_host != nullptr) {
        m_host->remove_window(*this);
        m_host = nullptr;
    }
}

void window_impl::set_event_handler(std::function<void(const event_message&)> handler) {
    m_event_handler = std::move(handler);
}

void window_impl::set_extension(std::shared_ptr<window_extension> extension) {
    m_extension = std::move(extension);
}

void window_impl::set_should_close(bool should_close) noexcept {
    m_should_close.store(should_close, std::memory_order_release);
}

bool window_impl::should_close() const noexcept {
    return m_should_close.load(std::memory_order_acquire);
}

bool window_impl::is_closed() const noexcept {
    return m_closed.load(std::memory_order_acquire);
}

void* window_impl::native_handle() const noexcept {
    return m_native;
}

void window_impl::set_min_size(window_extent size) {
    require_extent(size);
    m_min_size = size;
}

void window_impl::set_max_size(window_extent size) {
    require_extent(size);
    m_max_size = size;
}

window_extent window_impl::min_size() const noexcept {
    return m_min_size;
}

window_extent window_impl::max_size() const noexcept {
    return m_max_size;
}

double window_impl::opacity() const noexcept {
    return m_opacity;
}

bool window_impl::resizable() const noexcept {
    return m_resizable;
}

bool window_impl::is_always_on_top() const noexcept {
    return m_always_on_top;
}

double window_impl::scale() const {
    return static_cast<double>(dpi()) / 96.0;
}

void window_impl::set_native_handle(void* native) noexcept {
    m_native = native;
}

void window_impl::set_opacity_value(double opacity) noexcept {
    m_opacity = opacity;
}

void window_impl::set_resizable_value(bool resizable) noexcept {
    m_resizable = resizable;
}

void window_impl::set_always_on_top_value(bool on) noexcept {
    m_always_on_top = on;
}

void window_impl::native_lost() noexcept {
    m_closed.store(true, std::memory_order_release);

    // The window is no longer registered with the app. Drop the owner as well:
    // the handle can outlive the app, and its destructor must not call back
    // into an app that may already be gone.
    app_impl* host = m_host;
    m_host = nullptr;
    if (host != nullptr) host->window_destroyed(*this);
}

void window_impl::mark_closed() noexcept {
    m_closed.store(true, std::memory_order_release);
}

void window_impl::detach_host() noexcept {
    m_host = nullptr;
}

bool window_impl::handle_extension(std::uint32_t id, std::uintptr_t argument1,
                                   std::intptr_t argument2,
                                   std::intptr_t& result) noexcept {
    // Copy: the hook may replace itself while it runs.
    const auto extension = m_extension;
    if (!extension) return false;

    try {
        return extension->handle_message(id, argument1, argument2, result);
    } catch (...) {
        return false;
    }
}

}  // namespace helios::detail
