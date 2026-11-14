#if defined(_WIN32)

#    include <HeliosView2/App.h>
#    include <HeliosView2/Event.h>
#    include <HeliosView2/Win32.h>
#    include <HeliosView2/Window.h>

#    include "test_support.h"

#    include <doctest/doctest.h>

#    include <cstdint>
#    include <cstring>
#    include <memory>
#    include <thread>
#    include <type_traits>

#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>

namespace {

template <class Event>
Event decode(const helios::event_message& message) {
    Event event{};
    const std::size_t size =
            message.size < sizeof(Event) ? message.size : sizeof(Event);
    std::memcpy(&event, message.data, size);
    return event;
}

}  // namespace

TEST_CASE("constructing a window creates a native window") {
    std::unique_ptr<helios::app> application = test_support::app();

    std::unique_ptr<helios::base_window> window =
            test_support::window(*application,
                                 {.title = "HeliosView2", .width = 320, .height = 240});
    REQUIRE(window != nullptr);

    CHECK(application->window_count() == 1);
    CHECK(window->native_handle() != nullptr);
    CHECK_FALSE(window->is_closed());
    CHECK_FALSE(window->is_visible());
    CHECK(window->title() == "HeliosView2");
    CHECK(window->is_enabled());
    CHECK(window->resizable());
    CHECK(window->opacity() == doctest::Approx(1.0));
    CHECK(window->dpi() >= 96);
    CHECK(window->scale() >= doctest::Approx(1.0));

    const helios::window_extent size = window->size();
    CHECK(size.width > 0);
    CHECK(size.height > 0);

    const helios::window_extent client = window->client_size();
    CHECK(client.width > 0);
    CHECK(client.height > 0);
    CHECK(client.width <= size.width);

    CHECK_NOTHROW(window->show());
    CHECK(window->is_visible());
    CHECK_NOTHROW(window->hide());
    CHECK_FALSE(window->is_visible());

    CHECK_NOTHROW(window->set_title("Renamed"));
    CHECK(window->title() == "Renamed");

    window->close();
    CHECK(window->is_closed());
    CHECK(window->native_handle() == nullptr);
    CHECK(application->window_count() == 0);

    // Closing the last window requests app shutdown (default option).
    std::int32_t exit_code = -1;
    CHECK_NOTHROW(application->run(exit_code));
    CHECK(exit_code == 0);
}

TEST_CASE("a window can be moved, sized and styled") {
    std::unique_ptr<helios::app> application = test_support::app();
    std::unique_ptr<helios::base_window> window =
            test_support::window(*application, {.title = "Geometry"});

    CHECK_NOTHROW(window->set_size(400, 300));
    CHECK(window->size() == helios::window_extent{400, 300});

    CHECK_NOTHROW(window->set_position(120, 90));
    CHECK(window->position() == std::pair<std::int32_t, std::int32_t>{120, 90});

    CHECK_NOTHROW(window->center());

    CHECK_NOTHROW(window->set_always_on_top(true));
    CHECK(window->is_always_on_top());

    // A hidden window only becomes topmost once it is shown.
    CHECK_NOTHROW(window->show());
    const HWND hwnd = static_cast<HWND>(window->native_handle());
    CHECK((::GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0);

    CHECK_NOTHROW(window->set_always_on_top(false));
    CHECK_FALSE(window->is_always_on_top());
    CHECK((::GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) == 0);
    CHECK_NOTHROW(window->hide());

    CHECK_NOTHROW(window->set_opacity(0.5));
    CHECK(window->opacity() == doctest::Approx(0.5));

    CHECK_NOTHROW(window->enable(false));
    CHECK_FALSE(window->is_enabled());
    CHECK_NOTHROW(window->enable(true));
    CHECK(window->is_enabled());

    CHECK_NOTHROW(window->set_resizable(false));
    CHECK_FALSE(window->resizable());
    CHECK_NOTHROW(window->set_resizable(true));
    CHECK(window->resizable());

    CHECK_NOTHROW(window->set_min_size({200, 150}));
    CHECK(window->min_size() == helios::window_extent{200, 150});
    CHECK_NOTHROW(window->set_min_size({}));
    CHECK(window->min_size() == helios::window_extent{});

    CHECK_NOTHROW(window->set_max_size({1024, 768}));
    CHECK(window->max_size() == helios::window_extent{1024, 768});
    CHECK_NOTHROW(window->set_max_size({}));

    CHECK_NOTHROW(window->request_attention());
}

TEST_CASE("a window rejects invalid geometry") {
    std::unique_ptr<helios::app> application = test_support::app();
    std::unique_ptr<helios::base_window> window =
            test_support::window(*application, {.title = "Invalid"});

    CHECK_THROWS_AS(window->set_size(0, 100), helios::error);
    CHECK_THROWS_AS(window->set_size(100, -1), helios::error);
    CHECK_THROWS_AS(window->set_opacity(1.5), helios::error);
    CHECK_THROWS_AS(window->set_min_size({-1, 0}), helios::error);
}

TEST_CASE("a window is movable") {
    static_assert(std::is_nothrow_move_constructible_v<helios::base_window>);
    static_assert(std::is_nothrow_move_assignable_v<helios::base_window>);
    static_assert(!std::is_copy_constructible_v<helios::base_window>);

    std::unique_ptr<helios::app> application = test_support::app();
    std::unique_ptr<helios::base_window> original =
            test_support::window(*application, {.title = "Move"});
    REQUIRE(original != nullptr);

    const void* native = original->native_handle();
    REQUIRE(native != nullptr);

    helios::base_window moved{std::move(*original)};
    CHECK(moved.native_handle() == native);
    CHECK(moved.title() == "Move");
    CHECK(application->window_count() == 1);

    // The moved-from handle is empty and reports it.
    CHECK_THROWS_AS(original->show(), helios::error);
    CHECK_THROWS_AS(static_cast<void>(original->is_closed()), helios::error);

    // Move assignment closes the window the target already owned.
    helios::base_window other{*application, {.title = "Other"}};
    CHECK(application->window_count() == 2);

    other = std::move(moved);
    CHECK(other.native_handle() == native);
    CHECK(other.title() == "Move");
    CHECK(application->window_count() == 1);

    other.close();
    CHECK(other.is_closed());
}

TEST_CASE("windows survive moving the application") {
    std::unique_ptr<helios::app> application = test_support::app();
    std::unique_ptr<helios::base_window> window =
            test_support::window(*application, {.title = "Moved app"});
    REQUIRE(window != nullptr);

    helios::app moved{std::move(*application)};

    CHECK(moved.window_count() == 1);
    CHECK(window->title() == "Moved app");
    CHECK_NOTHROW(window->set_title("Still works"));
    CHECK(window->title() == "Still works");

    window->close();
    CHECK(moved.window_count() == 0);
}

TEST_CASE("a window outlives the app that created it") {
    std::unique_ptr<helios::app> application = test_support::app();

    std::unique_ptr<helios::base_window> window =
            test_support::window(*application, {.title = "Lifetime"});
    REQUIRE(window != nullptr);

    application.reset();
    CHECK(window->is_closed());
    CHECK(window->native_handle() == nullptr);

    window.reset();
}

TEST_CASE("window operations report the wrong thread") {
    std::unique_ptr<helios::app> application = test_support::app();
    std::unique_ptr<helios::base_window> window =
            test_support::window(*application, {.title = "Thread"});
    REQUIRE(window != nullptr);

    std::thread worker([&] {
        try {
            window->show();
            FAIL("show should have thrown");
        } catch (const helios::error& failure) {
            CHECK(failure.code() == helios::wrong_thread);
            CHECK(failure.frame().context == "showing the window");
        }

        try {
            window->set_size(100, 100);
            FAIL("set_size should have thrown");
        } catch (const helios::error& failure) {
            CHECK(failure.code() == helios::wrong_thread);
            CHECK(failure.frame().context == "resizing the window");
        }
    });
    worker.join();

    CHECK_NOTHROW(window->close());
}

TEST_CASE("a window delivers its events to the handler") {
    std::unique_ptr<helios::app> application = test_support::app();

    helios::base_window window{*application, {.title = "Events"}};
    REQUIRE(window.native_handle() != nullptr);

    int resizes = 0;
    int moves = 0;
    int mouse_moves = 0;
    int mouse_leaves = 0;
    int focus_events = 0;
    bool last_focus = false;
    int activated_events = 0;
    int deactivated_events = 0;
    int minimize_events = 0;
    int maximize_events = 0;
    int restore_events = 0;
    int show_events = 0;
    int hide_events = 0;
    int text_events = 0;
    std::uint32_t text_codepoint = 0;
    int dpi_events = 0;
    std::uint32_t dpi_value = 0;
    int display_events = 0;
    int theme_events = 0;
    int settings_events = 0;
    bool closed = false;
    bool destroyed = false;
    std::uint32_t key = 0;

    window.set_event_handler([&](const helios::event_message& message) {
        switch (message.kind) {
        case helios::event_kind::resize:
            ++resizes;
            break;
        case helios::event_kind::move:
            ++moves;
            break;
        case helios::event_kind::mouse: {
            const auto event = decode<helios::mouse_event>(message);
            if (event.action == helios::mouse_action::move) ++mouse_moves;
            break;
        }
        case helios::event_kind::mouse_leave:
            ++mouse_leaves;
            break;
        case helios::event_kind::focus: {
            const auto event = decode<helios::focus_event>(message);
            ++focus_events;
            last_focus = event.focused != 0;
            break;
        }
        case helios::event_kind::activate: {
            const auto event = decode<helios::activate_event>(message);
            if (event.active) {
                ++activated_events;
            } else {
                ++deactivated_events;
            }
            break;
        }
        case helios::event_kind::minimize:
            ++minimize_events;
            break;
        case helios::event_kind::maximize:
            ++maximize_events;
            break;
        case helios::event_kind::restore:
            ++restore_events;
            break;
        case helios::event_kind::show:
            ++show_events;
            break;
        case helios::event_kind::hide:
            ++hide_events;
            break;
        case helios::event_kind::text: {
            const auto event = decode<helios::text_event>(message);
            ++text_events;
            text_codepoint = event.codepoint;
            break;
        }
        case helios::event_kind::dpi: {
            const auto event = decode<helios::dpi_event>(message);
            ++dpi_events;
            dpi_value = event.dpi;
            break;
        }
        case helios::event_kind::display_change:
            ++display_events;
            break;
        case helios::event_kind::theme_change:
            ++theme_events;
            break;
        case helios::event_kind::settings_change:
            ++settings_events;
            break;
        case helios::event_kind::key:
            key = decode<helios::key_event>(message).virtual_key;
            break;
        case helios::event_kind::close:
            closed = true;
            break;
        case helios::event_kind::destroy:
            destroyed = true;
            break;
        default:
            break;
        }
    });

    const HWND hwnd = static_cast<HWND>(window.native_handle());

    ::SetWindowPos(hwnd, nullptr, 40, 50, 400, 300, SWP_NOZORDER | SWP_NOACTIVATE);
    CHECK(resizes > 0);
    CHECK(moves > 0);

    ::SendMessageW(hwnd, WM_SHOWWINDOW, TRUE, 0);
    CHECK(show_events == 1);
    ::SendMessageW(hwnd, WM_SHOWWINDOW, FALSE, 0);
    CHECK(hide_events == 1);

    ::SendMessageW(hwnd, WM_SIZE, SIZE_MINIMIZED, 0);
    CHECK(minimize_events == 1);
    ::SendMessageW(hwnd, WM_SIZE, SIZE_MAXIMIZED, MAKELPARAM(400, 300));
    CHECK(maximize_events == 1);
    ::SendMessageW(hwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(400, 300));
    CHECK(restore_events == 1);

    ::SendMessageW(hwnd, WM_ACTIVATE, WA_ACTIVE, 0);
    CHECK(activated_events >= 1);
    ::SendMessageW(hwnd, WM_ACTIVATE, WA_INACTIVE, 0);
    CHECK(deactivated_events >= 1);

    ::SendMessageW(hwnd, WM_SETFOCUS, 0, 0);
    CHECK(last_focus);
    ::SendMessageW(hwnd, WM_KILLFOCUS, 0, 0);
    CHECK_FALSE(last_focus);
    CHECK(focus_events >= 2);

    ::SendMessageW(hwnd, WM_CHAR, 'x', 1);
    CHECK(text_events == 1);
    CHECK(text_codepoint == 'x');

    RECT suggested{10, 20, 300, 200};
    ::SendMessageW(hwnd, WM_DPICHANGED, MAKELONG(144, 144),
                   reinterpret_cast<LPARAM>(&suggested));
    CHECK(dpi_events == 1);
    CHECK(dpi_value == 144);

    ::SendMessageW(hwnd, WM_DISPLAYCHANGE, 32, MAKELPARAM(1920, 1080));
    CHECK(display_events == 1);

    ::SendMessageW(hwnd, WM_THEMECHANGED, 0, 0);
    CHECK(theme_events == 1);

    ::SendMessageW(hwnd, WM_SETTINGCHANGE, SPI_SETWORKAREA, 0);
    CHECK(settings_events == 1);

    ::SendMessageW(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(10, 20));
    CHECK(mouse_moves == 1);
    ::SendMessageW(hwnd, WM_MOUSELEAVE, 0, 0);
    CHECK(mouse_leaves == 1);

    ::SendMessageW(hwnd, WM_KEYDOWN, 'A', 1);
    CHECK(key == 'A');

    ::SendMessageW(hwnd, WM_CLOSE, 0, 0);
    CHECK(closed);
    CHECK(destroyed);
    CHECK(window.is_closed());
    CHECK(application->window_count() == 0);
}

TEST_CASE("a close handler can veto the close request") {
    std::unique_ptr<helios::app> application = test_support::app();

    helios::base_window window{*application, {.title = "Veto"}};
    REQUIRE(window.native_handle() != nullptr);

    int close_events = 0;
    window.set_event_handler([&](const helios::event_message& message) {
        if (message.kind == helios::event_kind::close) {
            ++close_events;
            window.set_should_close(false);
        }
    });

    const HWND hwnd = static_cast<HWND>(window.native_handle());
    ::SendMessageW(hwnd, WM_CLOSE, 0, 0);

    CHECK(close_events == 1);
    CHECK_FALSE(window.is_closed());
    CHECK(application->window_count() == 1);

    window.close();
    CHECK(window.is_closed());
}

TEST_CASE("the Win32 extension hook sees raw messages") {
    std::unique_ptr<helios::app> application = test_support::app();
    helios::base_window window{*application, {.title = "Native"}};
    const HWND hwnd = static_cast<HWND>(window.native_handle());
    REQUIRE(hwnd != nullptr);

    bool seen = false;
    helios::win32::set_native_message_handler(
            window, [&](helios::win32::native_message& message) {
                if (message.id != WM_APP + 7) return;
                seen = true;
                CHECK(message.hwnd == hwnd);
                message.handled = true;
                message.result = 42;
            });

    CHECK(::SendMessageW(hwnd, WM_APP + 7, 0, 0) == 42);
    CHECK(seen);

    // Clearing the hook restores the default procedure.
    helios::win32::set_native_message_handler(window, {});
    CHECK(::SendMessageW(hwnd, WM_APP + 7, 0, 0) == 0);
}

#endif