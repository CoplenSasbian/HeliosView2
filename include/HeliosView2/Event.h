#pragma once



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
    count = 7,
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

struct event_message {
    event_kind kind = event_kind::close;
    const void* data = nullptr;
    std::size_t size = 0;
};




}  // namespace helios
