#pragma once

// Reflection based event dispatch.
//
// Annotate member functions of a window with [[= helios::event_handler]], then
// install the scanner as the window's event handler:
//
//     class editor_window final : public helios::base_window {
//     public:
//         using base_window::base_window;
//
//         [[= helios::event_handler]]
//         void on_resize(const helios::resize_event& event) { ... }
//     };
//
//     editor_window window{application, options};
//     window.set_event_handler(helios::handler_scanner<editor_window>{window});
//
// The scanner reflects over the members of Window; every annotated function
// whose single parameter is an event type from Event.h is a handler for that
// parameter's kind. When a message of that kind arrives, the payload is copied
// into the event struct and the member is called. Handlers run in declaration
// order, and several may serve the same kind.
//
// The annotated handlers have to be reachable from the scanner: keep them
// public, or add
//
//     template <class> friend class helios::handler_scanner;
//
// to the window when they must stay private.

#include <HeliosView2/Event.h>
#include <HeliosView2/HeliosView2.h>

#include <concepts>
#include <cstddef>
#include <cstring>
#include <type_traits>

#if HELIOSVIEW2_HAS_REFLECTION
#    include <meta>
#    include <span>
#endif

namespace helios {

/// A type usable as an annotated handler parameter: one of the event structs
/// from Event.h, recognised by its static `kind`.
template <class Event>
concept window_event = requires {
    { Event::kind } -> std::convertible_to<event_kind>;
};

template <class Window>
class handler_scanner {
public:
    explicit handler_scanner(Window& window) noexcept : m_window(&window) {}

    void operator()(const event_message& message) const noexcept {
        dispatch(message);
    }

    [[nodiscard]] Window& window() const noexcept { return *m_window; }

private:
#if HELIOSVIEW2_HAS_REFLECTION

    void dispatch(const event_message& message) const noexcept {
        template for (constexpr auto member :
                      std::define_static_array(std::meta::members_of(
                              ^^Window, std::meta::access_context::unchecked()))) {
            constexpr bool annotated = std::meta::is_function(member)
                    && (std::meta::parent_of(member) == ^^Window)
                    && !std::meta::annotations_of_with_type(
                               member, ^^event_handler_tag).empty();
            if constexpr (annotated) {
                dispatch_to<member>(message);
            }
        }
    }

    template <std::meta::info Member>
    void dispatch_to(const event_message& message) const noexcept {
        if constexpr (std::meta::parameters_of(Member).size() != 1) {
            static_assert(std::meta::parameters_of(Member).size() == 1,
                          "an annotated event handler takes exactly one event argument");
        } else {
            using parameter_type = typename[: std::meta::type_of(
                    std::meta::parameters_of(Member)[0]) :];
            using event_type = std::remove_cvref_t<parameter_type>;

            if constexpr (window_event<event_type>) {
                if (message.kind != event_type::kind || message.data == nullptr) return;

                event_type event{};
                const std::size_t size =
                        message.size < sizeof(event_type) ? message.size : sizeof(event_type);
                std::memcpy(&event, message.data, size);

                (m_window->[: Member :])(event);
            } else {
                static_assert(window_event<event_type>,
                              "an annotated event handler parameter must be an event type from Event.h");
            }
        }
    }

#else

    void dispatch(const event_message&) const noexcept {
        static_assert(!std::is_same_v<Window, Window>,
                      "handler_scanner needs C++26 reflection: compile this translation unit "
                      "with -freflection (GCC 16+)");
    }

#endif

    Window* m_window;
};

}  // namespace helios
