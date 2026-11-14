#pragma once

// Example window, separated from its implementation on purpose: the class is
// fully declared here, so the constructor definition in example_window.cpp can
// reflect over it and install handler_scanner for the annotated members.

#include <HeliosView2/App.h>
#include <HeliosView2/Event.h>
#include <HeliosView2/Window.h>

class example_window final : public helios::base_window {
public:
    explicit example_window(helios::app& host,
                            const helios::window_options& options = {});

    example_window(example_window&& other) noexcept;
    example_window& operator=(example_window&& other) noexcept;
    /// The name is free; the scanner picks the event kind from the parameter.
    [[= helios::event_handler]]
    void on_close(const helios::close_event& event);

    [[= helios::event_handler]]
    void on_resize(const helios::resize_event& event);

    [[= helios::event_handler]]
    void on_paint(const helios::paint_event& event);

    [[= helios::event_handler]]
    void on_activate(const helios::activate_event& event);

    [[= helios::event_handler]]
    void on_text(const helios::text_event& event);

    [[= helios::event_handler]]
    void on_minimize(const helios::minimize_event& event);

    [[= helios::event_handler]]
    void on_maximize(const helios::maximize_event& event);

    [[= helios::event_handler]]
    void on_restore(const helios::restore_event& event);

    [[= helios::event_handler]]
    void on_dpi(const helios::dpi_event& event);
};
