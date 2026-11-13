#include "../AppBackend.h"

#include <HeliosView2/App.h>

namespace helios {

std::unique_ptr<app> create_app(const app_options& options) {
    return detail::create_platform_app(options);
}

}  // namespace helios
