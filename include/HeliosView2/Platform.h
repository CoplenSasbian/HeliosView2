#pragma once

// Platform extension plumbing.
//
// The portable window API (Window.h) never mentions a platform. Platform
// headers such as HeliosView2/Win32.h attach their own state to a window
// through detail::window_access, so raw message ids and arguments stay in the
// platform header.

#include <HeliosView2/HeliosView2_export.h>

#include <cstdint>
#include <memory>

namespace helios {

class base_window;

namespace detail {

/// Backend state a platform header can attach to a window.
struct window_extension {
    virtual ~window_extension() = default;

    /// Offers one raw platform message; returns true when handled and leaves
    /// the result in `result`.
    virtual bool handle_message(std::uint32_t id, std::uintptr_t argument1,
                                std::intptr_t argument2, std::intptr_t& result) {
        (void) id;
        (void) argument1;
        (void) argument2;
        (void) result;
        return false;
    }
};

/// Access for platform headers. Not part of the portable window API.
struct HELIOSVIEW2_EXPORT window_access {
    static void set_extension(base_window& window,
                              std::shared_ptr<window_extension> extension);
};

}  // namespace detail

}  // namespace helios
