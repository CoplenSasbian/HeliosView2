#pragma once

// Helpers shared by the tests: a failing create throws, so the tests that expect
// success simply call through.

#include <HeliosView2/App.h>
#include <HeliosView2/Window.h>

#include <memory>
#include <utility>

namespace test_support {

inline std::unique_ptr<helios::app> app() {
    return helios::create_app(helios::app_options{});
}

inline std::shared_ptr<helios::base_window> window(helios::app& target,
                                                   helios::window_options options = {}) {
    return target.create_window(options);
}

}  // namespace test_support
