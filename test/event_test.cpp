#include <HeliosView2/Event.h>

#include <doctest/doctest.h>

#include <cstdint>
#include <cstring>

TEST_CASE("event catalog is dense") {
    static_assert(static_cast<std::uint16_t>(helios::event_kind::close) == 0);
    static_assert(static_cast<std::uint16_t>(helios::event_kind::resize) == 1);
    static_assert(static_cast<std::uint16_t>(helios::event_kind::move) == 2);
    static_assert(static_cast<std::uint16_t>(helios::event_kind::focus) == 3);
    static_assert(static_cast<std::uint16_t>(helios::event_kind::mouse) == 4);
    static_assert(static_cast<std::uint16_t>(helios::event_kind::key) == 5);
    static_assert(static_cast<std::uint16_t>(helios::event_kind::paint) == 6);
    static_assert(static_cast<std::uint16_t>(helios::event_kind::show) == 7);
    static_assert(static_cast<std::uint16_t>(helios::event_kind::hide) == 8);
    static_assert(static_cast<std::uint16_t>(helios::event_kind::minimize) == 9);
    static_assert(static_cast<std::uint16_t>(helios::event_kind::maximize) == 10);
    static_assert(static_cast<std::uint16_t>(helios::event_kind::restore) == 11);
    static_assert(static_cast<std::uint16_t>(helios::event_kind::activate) == 12);
    static_assert(static_cast<std::uint16_t>(helios::event_kind::text) == 13);
    static_assert(static_cast<std::uint16_t>(helios::event_kind::mouse_leave) == 14);
    static_assert(static_cast<std::uint16_t>(helios::event_kind::dpi) == 15);
    static_assert(static_cast<std::uint16_t>(helios::event_kind::display_change) == 16);
    static_assert(static_cast<std::uint16_t>(helios::event_kind::theme_change) == 17);
    static_assert(static_cast<std::uint16_t>(helios::event_kind::settings_change) == 18);
    static_assert(static_cast<std::uint16_t>(helios::event_kind::destroy) == 19);
    static_assert(static_cast<std::uint16_t>(helios::event_kind::count) == 20);

    CHECK(helios::close_event::kind == helios::event_kind::close);
    CHECK(helios::paint_event::kind == helios::event_kind::paint);
    CHECK(helios::destroy_event::kind == helios::event_kind::destroy);
}

TEST_CASE("event payloads are plain values") {
    const helios::resize_event resize{320, 240};
    CHECK(resize.width == 320);
    CHECK(resize.height == 240);
    CHECK(helios::resize_event::kind == helios::event_kind::resize);

    const helios::key_event key{.virtual_key = 65, .scan_code = 30, .repeat = 3};
    CHECK(key.virtual_key == 65);
    CHECK(key.action == helios::key_action::down);

    const helios::activate_event active{.active = true};
    CHECK(active.active);

    const helios::text_event text{.codepoint = U'λ'};
    CHECK(text.codepoint == U'λ');

    const helios::dpi_event dpi{.dpi = 144, .x = 10, .y = 20, .width = 800, .height = 600};
    CHECK(dpi.dpi == 144);
    CHECK(dpi.width == 800);
}

TEST_CASE("event_message borrows a typed payload") {
    const helios::resize_event resize{640, 480};
    const helios::event_message message{helios::resize_event::kind, &resize,
                                        sizeof(helios::resize_event)};

    CHECK(message.kind == helios::event_kind::resize);
    REQUIRE(message.size == sizeof(helios::resize_event));

    helios::resize_event copy{};
    std::memcpy(&copy, message.data, message.size);

    CHECK(copy.width == 640);
    CHECK(copy.height == 480);
}

TEST_CASE("modifier flags compose") {
    const auto modifiers = helios::key_modifier::shift
                         | helios::key_modifier::control;

    CHECK(static_cast<std::uint32_t>(modifiers)
          == (static_cast<std::uint32_t>(helios::key_modifier::shift)
              | static_cast<std::uint32_t>(helios::key_modifier::control)));
}
