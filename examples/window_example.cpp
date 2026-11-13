#include <HeliosView2/App.h>
#include <HeliosView2/Window.h>

#include <cstdio>
#include <cstring>

namespace abi = helios::abi;

namespace {

void print_status(const char* operation, abi::status status) {
    std::fprintf(stderr, "%s failed: status=%d\n", operation, status);
}

}  // namespace

int main(int argc, char** argv) {
    const bool smoke = argc > 1 && std::strcmp(argv[1], "--smoke") == 0;

    helios::app_options app_options{};
    app_options.id = "com.example.heliosview2.window";
    app_options.id_size = std::strlen(app_options.id);
    app_options.name = "HeliosView2 Window Example";
    app_options.name_size = std::strlen(app_options.name);
    app_options.version = "0.1.0";
    app_options.version_size = std::strlen(app_options.version);
    app_options.quit_on_last_window_closed = 1;

    abi::ptr<helios::app> application;
    abi::status status = helios::create_app(&app_options, std::out_ptr(application));
    if (abi::failed(status) || application == nullptr) {
        print_status("create_app", status);
        return 1;
    }

    abi::window_options window_options{};
    window_options.title = "HeliosView2 Window Example";
    window_options.title_size = std::strlen(window_options.title);
    window_options.width = 960;
    window_options.height = 600;
    window_options.resizable = 1;
    window_options.visible = smoke ? 0 : 1;

    abi::ptr<helios::base_window> window;
    status = application->create_window(&window_options, std::out_ptr(window));
    if (abi::failed(status) || window == nullptr) {
        print_status("create_window", status);
        return 1;
    }

    if (smoke) {
        status = window->close();
        if (abi::failed(status)) {
            print_status("close", status);
            return 1;
        }
    }

    std::int32_t exit_code = 0;
    status = application->run(&exit_code);
    if (abi::failed(status)) {
        print_status("run", status);
        return 1;
    }

    if (!smoke) {
        std::printf("window example exited with code %d\n", exit_code);
    }
    return exit_code;
}
