#include <HeliosView2/Event.h>

#include <doctest/doctest.h>

#include <cstring>

TEST_CASE("event catalog is dense and POD") {
    static_assert(static_cast<std::uint16_t>(helios::event_kind::close) == 0);
    static_assert(static_cast<std::uint16_t>(helios::event_kind::resize) == 1);
    static_assert(static_cast<std::uint16_t>(helios::event_kind::move) == 2);
    static_assert(static_cast<std::uint16_t>(helios::event_kind::focus) == 3);
    static_assert(static_cast<std::uint16_t>(helios::event_kind::mouse) == 4);
    static_assert(static_cast<std::uint16_t>(helios::event_kind::key) == 5);
    static_assert(static_cast<std::uint16_t>(helios::event_kind::paint) == 6);
    static_assert(static_cast<std::uint16_t>(helios::event_kind::count) == 7);

    CHECK(helios::close_event::kind == helios::event_kind::close);
    CHECK(helios::paint_event::kind == helios::event_kind::paint);
}

TEST_CASE("event_message carries a resize payload") {
    helios::resize_event event{320, 240};
    helios::event_message message{
            helios::resize_event::kind, &event, sizeof(event)};

    CHECK(message.kind == helios::event_kind::resize);
    REQUIRE(message.size == sizeof(helios::resize_event));

    helios::resize_event copy{};
    std::memcpy(&copy, message.data, message.size);

    CHECK(copy.width == 320);
    CHECK(copy.height == 240);
}

TEST_CASE("modifier flags compose") {
    const auto modifiers = helios::key_modifier::shift
                         | helios::key_modifier::control;

    CHECK(static_cast<std::uint32_t>(modifiers)
          == (static_cast<std::uint32_t>(helios::key_modifier::shift)
              | static_cast<std::uint32_t>(helios::key_modifier::control)));
}
