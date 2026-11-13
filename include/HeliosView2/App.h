#pragma once

// Public application interface. Platform implementations live in the DLL; the
// factory below is the only exported entry point.

#include <HeliosView2/HeliosView2_export.h>
#include <HeliosView2/abi/object.h>

#include <cstddef>
#include <cstdint>

namespace helios {

enum class activation_policy : std::int32_t {
    regular = 0,
    accessory = 1,
    prohibited = 2,
};

struct app_options {
    const char* id = nullptr;
    std::size_t id_size = 0;
    const char* name = nullptr;
    std::size_t name_size = 0;
    const char* version = nullptr;
    std::size_t version_size = 0;
    std::uint8_t quit_on_last_window_closed = 1;
    activation_policy policy = activation_policy::regular;
};

namespace abi {
struct window_options;
class task;
}

class base_window;

class app : public abi::unknown {
public:
    static constexpr abi::guid iid = abi::make_guid("helios.abi.app/1");

    virtual abi::status run(std::int32_t* exit_code) noexcept = 0;
    virtual void quit() noexcept = 0;
    [[nodiscard]] virtual bool is_running() const noexcept = 0;
    [[nodiscard]] virtual std::uint64_t ui_thread_id() const noexcept = 0;
    virtual abi::status create_window(const abi::window_options* options,
                                      base_window** out) noexcept = 0;
    [[nodiscard]] virtual std::uint64_t window_count() const noexcept = 0;

    // Adds a reference while the task is queued. The caller keeps its own
    // reference and may release it as soon as post returns.
    virtual abi::status post(abi::task* work) noexcept = 0;

protected:
    ~app() = default;
};

// No inline body: the implementation lives in the DLL.
extern "C" HELIOSVIEW2_EXPORT abi::status HELIOS_CALL
create_app(const app_options* options, app** out) noexcept;

}  // namespace helios
