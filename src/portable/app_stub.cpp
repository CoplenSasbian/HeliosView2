#include "../AppBackend.h"

namespace helios::detail {

std::unique_ptr<app> create_platform_app(const app_options&) {
    throw make_error(unsupported, "create_app", "this build has no windowing backend");
}

}  // namespace helios::detail
