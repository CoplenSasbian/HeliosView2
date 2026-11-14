#include "example_window.h"

#include <HeliosView2/HandlerScanner.h>
#include <HeliosView2/Log.h>

#ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#    define NOMINMAX
#endif
#include <windows.h>

namespace {

constexpr auto group_ref = helios::log::grp("example_window");

}  // namespace

example_window::example_window(helios::app& host, const helios::window_options& options)
    : helios::base_window(host, options) {
    // example_window is complete here - the handlers are already declared with
    // their [[= helios::event_handler]] annotations - so the scanner can
    // enumerate them from this translation unit.
    set_event_handler(helios::handler_scanner{*this});

    set_min_size({320, 200});
}

example_window::example_window(example_window&& other) noexcept : base_window(std::move(other))
{
    set_event_handler(helios::handler_scanner{*this});
}

example_window& example_window::operator=(example_window&& other) noexcept
{
    base_window::operator=(std::move(other));
    set_event_handler(helios::handler_scanner{*this});
    return *this;
}


void example_window::on_close(const helios::close_event&) {
    group_ref.info("close requested");
}

void example_window::on_resize(const helios::resize_event& event) {
    group_ref.info("resized to {}x{}", event.width, event.height);
}

void example_window::on_activate(const helios::activate_event& event) {
    group_ref.info("{}", event.active ? "activated" : "deactivated");
}

void example_window::on_text(const helios::text_event& event) {
    group_ref.info("text input: U+{:04X} '{}'", event.codepoint,
                   static_cast<char>(event.codepoint));
}

void example_window::on_minimize(const helios::minimize_event&) {
    group_ref.info("minimized");
}

void example_window::on_maximize(const helios::maximize_event&) {
    group_ref.info("maximized");
}

void example_window::on_restore(const helios::restore_event&) {
    group_ref.info("restored");
}

void example_window::on_dpi(const helios::dpi_event& event) {
    group_ref.info("dpi changed to {} (suggested {}x{} at {}, {})", event.dpi,
                   event.width, event.height, event.x, event.y);
}

void example_window::on_paint(const helios::paint_event&) {
    const auto hwnd = static_cast<HWND>(native_handle());
    PAINTSTRUCT paint{};
    HDC dc = ::BeginPaint(hwnd, &paint);

    RECT area{};
    ::GetClientRect(hwnd, &area);
    ::FillRect(dc, &area, static_cast<HBRUSH>(::GetStockObject(WHITE_BRUSH)));
    ::SetBkMode(dc, TRANSPARENT);

    const wchar_t* text = L"HeliosView2: annotate handlers, scan them with reflection";
    ::DrawTextW(dc, text, -1, &area, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    ::EndPaint(hwnd, &paint);
}
