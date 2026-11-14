#include "example_window.h"

#include <HeliosView2/Error.h>
#include <HeliosView2/Log.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

constexpr auto group_ref = helios::log::grp("main");

}  // namespace

int main(int argc, char** argv) {
    helios::log::set_level(helios::log::level::trace);
    try {
        (void) helios::log::add_chain({
                .minimum_level = helios::log::level::trace,
                .formatter = helios::log::pattern_formatter(
                        "{t:%H:%M:%S} [{l:^7}] {g} | {m}  - {p}"),
                .sink = helios::log::console_sink{true},
        });
    } catch (const helios::error& failure) {
        std::fprintf(stderr, "logging is unavailable: %s\n", failure.what());
    }

    const bool smoke = argc > 1 && std::strcmp(argv[1], "--smoke") == 0;

    try {
        // A failing call throws with everything: what, where and why.
        helios::app application(helios::app_options{
                .id = "com.example.heliosview2.window",
                .name = "HeliosView2 Window Example",
                .version = "0.1.0",
        });

        example_window window{application,
                              {
                                      .title = "HeliosView2 Window Example",
                                      .width = 960,
                                      .height = 600,
                                      .visible = !smoke,
                                      .centered = true,
                              }};

        if (smoke) window.close();

        std::int32_t exit_code = 0;
        application.run(exit_code);

        if (!smoke) group_ref.info("window example exited with code {}", exit_code);
        helios::log::flush();
        return exit_code;
    } catch (const helios::error& failure) {
        group_ref.critical("{}", failure);
        group_ref.debug("{:json}", failure);
        helios::log::flush();
        return failure.code();
    }
}
