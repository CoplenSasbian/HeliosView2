#include "app_factory.h"

namespace helios {

extern "C" HELIOSVIEW2_EXPORT abi::status HELIOS_CALL
create_app(const app_options* options, app** out) noexcept {
    if (options == nullptr || out == nullptr) return abi::invalid_argument;
    *out = nullptr;
    return detail::create_platform_app(*options, out);
}

}  // namespace helios
