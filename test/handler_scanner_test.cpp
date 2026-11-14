#include <HeliosView2/App.h>
#include <HeliosView2/Event.h>
#include <HeliosView2/HandlerScanner.h>
#include <HeliosView2/Window.h>

#include "test_support.h"

#include <doctest/doctest.h>

#include <cstdint>
#include <cstring>
#include <memory>

#if defined(_WIN32)
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

namespace {

template <class Event>
helios::event_message message_of(const Event& event) {
    return {Event::kind, &event, sizeof(Event)};
}

struct fake_window {
    [[= helios::event_handler]]
    void on_resize(const helios::resize_event& event) {
        ++resizes;
        width = event.width;
        height = event.height;
    }

    [[= helios::event_handler]]
    void on_key(const helios::key_event& event) {
        virtual_key = event.virtual_key;
    }

    // Not annotated: the scanner must leave it alone.
    void on_mouse(const helios::mouse_event&) {
        ++unannotated_calls;
    }

    int resizes = 0;
    int width = 0;
    int height = 0;
    std::uint32_t virtual_key = 0;
    int unannotated_calls = 0;
};

/// Handlers may stay private when the window names the scanner as a friend.
class private_window {
    template <class>
    friend class helios::handler_scanner;

    [[= helios::event_handler]]
    void on_close(const helios::close_event&) {
        ++closes;
    }

public:
    int closes = 0;
};

#if defined(_WIN32)
class scanned_window final : public helios::base_window {
public:
    explicit scanned_window(helios::app& host,
                            const helios::window_options& options = {});

    int resizes = 0;
    int closes = 0;
    std::int32_t width = 0;
    std::uint32_t virtual_key = 0;

    [[= helios::event_handler]]
    void on_resize(const helios::resize_event& event) {
        ++resizes;
        width = event.width;
    }

    [[= helios::event_handler]]
    void on_key(const helios::key_event& event) {
        virtual_key = event.virtual_key;
    }

    [[= helios::event_handler]]
    void on_close(const helios::close_event&) {
        ++closes;
    }
};

// Defined after the class, exactly like a window that installs its own
// scanner from the constructor in a .cpp file.
scanned_window::scanned_window(helios::app& host, const helios::window_options& options)
    : helios::base_window(host, options) {
    set_event_handler(helios::handler_scanner<scanned_window>{*this});
}
#endif

}  // namespace

TEST_CASE("the scanner calls annotated handlers by event kind") {
    fake_window window;
    const helios::handler_scanner<fake_window> scan{window};

    const helios::resize_event resize{320, 240};
    scan(message_of(resize));

    CHECK(window.resizes == 1);
    CHECK(window.width == 320);
    CHECK(window.height == 240);

    const helios::mouse_event mouse{};
    scan(message_of(mouse));
    CHECK(window.unannotated_calls == 0);

    const helios::key_event key{.virtual_key = 65};
    scan(message_of(key));
    CHECK(window.virtual_key == 65);
}

TEST_CASE("the scanner only calls handlers whose kind matches") {
    fake_window window;
    const helios::handler_scanner<fake_window> scan{window};

    const helios::close_event close{};
    scan(message_of(close));

    CHECK(window.resizes == 0);
    CHECK(window.virtual_key == 0);
}

TEST_CASE("a private handler can befriend the scanner") {
    private_window window;
    const helios::handler_scanner<private_window> scan{window};

    const helios::close_event close{};
    scan(message_of(close));

    CHECK(window.closes == 1);
}

#if defined(_WIN32)
TEST_CASE("a scanner dispatches a real window's events") {
    std::unique_ptr<helios::app> application = test_support::app();

    // The window installs its own scanner in its constructor.
    scanned_window window{*application, {.title = "Scanned"}};

    const HWND hwnd = static_cast<HWND>(window.native_handle());
    REQUIRE(hwnd != nullptr);

    ::SetWindowPos(hwnd, nullptr, 0, 0, 500, 400, SWP_NOZORDER | SWP_NOACTIVATE);
    CHECK(window.resizes > 0);
    CHECK(window.width > 0);

    ::SendMessageW(hwnd, WM_KEYDOWN, 'A', 1);
    CHECK(window.virtual_key == 'A');

    ::SendMessageW(hwnd, WM_CLOSE, 0, 0);
    CHECK(window.closes == 1);
    CHECK(window.is_closed());
}
#endif
