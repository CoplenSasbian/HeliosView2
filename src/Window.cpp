#include <HeliosView2/App.h>
#include <HeliosView2/Window.h>

#include "WindowImpl.h"

#include <utility>

namespace helios {

base_window::base_window(app& host, const window_options& options)
    : m_impl(create_window_impl(host.impl(), options)) {}

base_window::base_window(base_window&& other) noexcept = default;

base_window& base_window::operator=(base_window&& other) noexcept = default;

base_window::~base_window() = default;

detail::window_impl& base_window::impl() const {
    if (!m_impl) {
        throw make_error(invalid_state, "using the window",
                         "the window has been moved from");
    }
    return *m_impl;
}

void base_window::show() {
    impl().show();
}

void base_window::hide() {
    impl().hide();
}

bool base_window::is_visible() const {
    return impl().is_visible();
}

void base_window::set_title(std::string_view title) {
    impl().set_title(title);
}

std::string base_window::title() const {
    return impl().title();
}

void base_window::set_size(std::int32_t width, std::int32_t height) {
    impl().set_size(width, height);
}

window_extent base_window::size() const {
    return impl().size();
}

window_extent base_window::client_size() const {
    return impl().client_size();
}

void base_window::set_position(std::int32_t x, std::int32_t y) {
    impl().set_position(x, y);
}

std::pair<std::int32_t, std::int32_t> base_window::position() const {
    return impl().position();
}

void base_window::center() {
    impl().center();
}

void base_window::minimize() {
    impl().minimize();
}

void base_window::maximize() {
    impl().maximize();
}

void base_window::restore() {
    impl().restore();
}

bool base_window::is_minimized() const {
    return impl().is_minimized();
}

bool base_window::is_maximized() const {
    return impl().is_maximized();
}

void base_window::focus() {
    impl().focus();
}

bool base_window::is_focused() const {
    return impl().is_focused();
}

void base_window::activate() {
    impl().activate();
}

void base_window::set_always_on_top(bool on) {
    impl().set_always_on_top(on);
}

bool base_window::is_always_on_top() const {
    return impl().is_always_on_top();
}

void base_window::set_opacity(double opacity) {
    impl().set_opacity(opacity);
}

double base_window::opacity() const {
    return impl().opacity();
}

void base_window::enable(bool enabled) {
    impl().enable(enabled);
}

bool base_window::is_enabled() const {
    return impl().is_enabled();
}

void base_window::request_attention() {
    impl().request_attention();
}

void base_window::set_resizable(bool resizable) {
    impl().set_resizable(resizable);
}

bool base_window::resizable() const {
    return impl().resizable();
}

void base_window::set_min_size(window_extent size) {
    impl().set_min_size(size);
}

void base_window::set_max_size(window_extent size) {
    impl().set_max_size(size);
}

window_extent base_window::min_size() const {
    return impl().min_size();
}

window_extent base_window::max_size() const {
    return impl().max_size();
}

std::uint32_t base_window::dpi() const {
    return impl().dpi();
}

double base_window::scale() const {
    return impl().scale();
}

void base_window::close() {
    impl().close();
}

void base_window::set_should_close(bool should_close) {
    impl().set_should_close(should_close);
}

bool base_window::should_close() const {
    return impl().should_close();
}

bool base_window::is_closed() const {
    return impl().is_closed();
}

void* base_window::native_handle() const {
    return impl().native_handle();
}

void base_window::set_event_handler(std::function<void(const event_message&)> handler) {
    impl().set_event_handler(std::move(handler));
}

namespace detail {

void window_access::set_extension(base_window& window,
                                  std::shared_ptr<window_extension> extension) {
    window.impl().set_extension(std::move(extension));
}

}  // namespace detail

}  // namespace helios
