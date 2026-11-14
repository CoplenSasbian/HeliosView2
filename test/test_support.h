#pragma once

// Helpers shared by the tests: a failing construction throws, so the tests that
// expect success simply construct through.

#include <HeliosView2/App.h>
#include <HeliosView2/Window.h>

#include <memory>
#include <utility>

namespace test_support {

inline std::unique_ptr<helios::app> app() {
    return std::make_unique<helios::app>();
}

inline std::unique_ptr<helios::base_window> window(helios::app& target,
                                                   helios::window_options options = {}) {
    return std::make_unique<helios::base_window>(target, options);
}

}  // namespace test_support
