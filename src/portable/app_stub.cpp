#include "../abi/app_factory.h"

namespace helios::detail {

abi::status create_platform_app(const app_options&, app** out) noexcept {
    if (out != nullptr) *out = nullptr;
    return abi::unsupported;
}

}  // namespace helios::detail
