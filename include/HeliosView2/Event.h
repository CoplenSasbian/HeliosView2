#pragma once

// Window events.
//
// base_window (Window.h) turns platform messages into these plain structs. The
// installed event handler receives an event_message that borrows the struct for
// the duration of the call; HandlerScanner.h turns annotated member functions
// into such a handler, so most windows never decode the message by hand.

#include <cstddef>
#include <cstdint>

namespace helios {

enum class event_kind : std::uint16_t {
    close = 0,
    resize = 1,
    move = 2,
    focus = 3,
    mouse = 4,
    key = 5,
    paint = 6,
    show = 7,
    hide = 8,
    minimize = 9,
    maximize = 10,
    restore = 11,
    activate = 12,
    text = 13,
    mouse_leave = 14,
    dpi = 15,
    display_change = 16,
    theme_change = 17,
    settings_change = 18,
    destroy = 19,
    count = 20,
};

enum class mouse_button : std::uint32_t {
    none = 0,
    left = 1,
    right = 2,
    middle = 3,
    x1 = 4,
    x2 = 5,
};

enum class mouse_action : std::uint32_t {
    move = 0,
    down = 1,
    up = 2,
    wheel = 3,
    double_click = 4,
};

enum class key_action : std::uint32_t {
    down = 0,
    up = 1,
    repeat = 2,
};

enum class key_modifier : std::uint32_t {
    none = 0,
    shift = 1u << 0,
    control = 1u << 1,
    alt = 1u << 2,
    super = 1u << 3,
    caps_lock = 1u << 4,
    num_lock = 1u << 5,
};

constexpr key_modifier operator|(key_modifier lhs, key_modifier rhs) noexcept {
    return static_cast<key_modifier>(static_cast<std::uint32_t>(lhs)
                                     | static_cast<std::uint32_t>(rhs));
}

constexpr key_modifier& operator|=(key_modifier& lhs, key_modifier rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

struct close_event {
    static constexpr event_kind kind = event_kind::close;
};

struct resize_event {
    static constexpr event_kind kind = event_kind::resize;

    /// Client area size; both are zero while the window is minimized.
    std::int32_t width = 0;
    std::int32_t height = 0;
};

struct move_event {
    static constexpr event_kind kind = event_kind::move;

    std::int32_t x = 0;
    std::int32_t y = 0;
};

struct focus_event {
    static constexpr event_kind kind = event_kind::focus;

    std::uint8_t focused = 0;
};

struct mouse_event {
    static constexpr event_kind kind = event_kind::mouse;

    std::int32_t x = 0;
    std::int32_t y = 0;
    mouse_button button = mouse_button::none;
    mouse_action action = mouse_action::move;
    std::int32_t wheel_delta = 0;
    std::int32_t horizontal_wheel_delta = 0;
    key_modifier modifiers = key_modifier::none;
};

struct key_event {
    static constexpr event_kind kind = event_kind::key;

    std::uint32_t virtual_key = 0;
    std::uint32_t scan_code = 0;
    key_action action = key_action::down;
    std::uint32_t repeat = 0;
    key_modifier modifiers = key_modifier::none;
};

struct paint_event {
    static constexpr event_kind kind = event_kind::paint;
};

struct show_event {
    static constexpr event_kind kind = event_kind::show;
};

struct hide_event {
    static constexpr event_kind kind = event_kind::hide;
};

struct minimize_event {
    static constexpr event_kind kind = event_kind::minimize;
};

struct maximize_event {
    static constexpr event_kind kind = event_kind::maximize;
};

struct restore_event {
    static constexpr event_kind kind = event_kind::restore;
};

struct activate_event {
    static constexpr event_kind kind = event_kind::activate;

    bool active = false;
};

/// A character produced by the keyboard, after TranslateMessage().
struct text_event {
    static constexpr event_kind kind = event_kind::text;

    std::uint32_t codepoint = 0;
};

struct mouse_leave_event {
    static constexpr event_kind kind = event_kind::mouse_leave;
};

/// The window moved to a different monitor or the monitor DPI changed; the
/// suggested bounds are in physical pixels.
struct dpi_event {
    static constexpr event_kind kind = event_kind::dpi;

    std::uint32_t dpi = 96;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
};

struct display_change_event {
    static constexpr event_kind kind = event_kind::display_change;

    std::uint32_t bits_per_pixel = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
};

struct theme_change_event {
    static constexpr event_kind kind = event_kind::theme_change;
};

/// WM_SETTINGCHANGE: `area` is the platform's subsystem mask (SPI_SETWORKAREA
/// and friends).
struct settings_change_event {
    static constexpr event_kind kind = event_kind::settings_change;

    std::uint32_t area = 0;
};

struct destroy_event {
    static constexpr event_kind kind = event_kind::destroy;
};

/// One event, as handed to base_window::set_event_handler. `data` points at the
/// matching event struct (resize_event, key_event, ...) and is only valid for
/// the duration of the handler call; `size` is that struct's size.
struct event_message {
    event_kind kind = event_kind::close;
    const void* data = nullptr;
    std::size_t size = 0;
};

/// Annotation marker for member functions that handler_scanner dispatches:
///
///     [[= helios::event_handler]]
///     void on_resize(const helios::resize_event& event) { ... }
struct event_handler_tag {};

inline constexpr event_handler_tag event_handler{};

}  // namespace helios
