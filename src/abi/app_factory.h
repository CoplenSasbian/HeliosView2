#pragma once

#include <HeliosView2/App.h>

namespace helios::detail {

abi::status create_platform_app(const app_options& options, app** out) noexcept;

}  // namespace helios::detail
