#pragma once

// glaze options shared by the modules that write JSON. glaze leaves control
// characters to its fast path by default, which emits NUL bytes and therefore
// invalid JSON; requesting escapes is the documented way to fix that.
//
// This is a detail header for .cpp files: including it does not make glaze part
// of the public interface.

#include <glaze/glaze.hpp>

namespace helios::detail {

struct json_opts : glz::opts {
    bool escape_control_characters = true;
};

}  // namespace helios::detail
