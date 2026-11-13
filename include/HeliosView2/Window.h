#pragma once

// Window ABI. The public surface is a single abstract base_window class;
// platform message handling stays behind the DLL boundary.

#include <HeliosView2/abi/object.h>

#include <cstddef>
#include <cstdint>
#include <limits>

namespace helios::abi {

struct window_options {
    const char* title = nullptr;
    std::size_t title_size = 0;

    std::int32_t width = 800;
    std::int32_t height = 600;
    std::int32_t x = std::numeric_limits<std::int32_t>::min();
    std::int32_t y = std::numeric_limits<std::int32_t>::min();

    std::uint8_t resizable = 1;
    std::uint8_t visible = 0;
};

}  // namespace helios::abi

namespace helios {

class base_window : public abi::unknown {
public:
    static constexpr abi::guid iid = abi::make_guid("helios.abi.window/1");

    virtual abi::status show() noexcept = 0;
    virtual abi::status hide() noexcept = 0;
    virtual abi::status close() noexcept = 0;
    virtual abi::status set_title(const char* utf8, std::size_t size) noexcept = 0;

    [[nodiscard]] virtual bool is_closed() const noexcept = 0;
    [[nodiscard]] virtual void* native_handle() const noexcept = 0;

protected:
    ~base_window() = default;
};

    
}  // namespace helios
