# Window example

Minimal entry point for the Win32 window backend, split into three files:

- `example_window.h` — the window class declaration, with the annotated handlers;
- `example_window.cpp` — its implementation; the constructor installs the
  reflection based handler after the class is complete;
- `main.cpp` — logging setup, app and window creation, message loop.

```cpp
// example_window.h
class example_window final : public helios::base_window {
public:
    explicit example_window(helios::app& host,
                            const helios::window_options& options = {});

    [[= helios::event_handler]]
    void on_resize(const helios::resize_event& event);
};
```

```cpp
// example_window.cpp
example_window::example_window(helios::app& host, const helios::window_options& options)
    : helios::base_window(host, options) {
    set_event_handler(helios::handler_scanner<example_window>{*this});
    set_min_size({320, 200});
}
```

The scanner reflects over the annotated members and calls the one whose event
kind matches. Events cover window lifecycle (`close`, `destroy`, `show`, `hide`,
`minimize`, `maximize`, `restore`, `activate`, `focus`), geometry (`resize`,
`move`), input (`mouse`, `mouse_leave`, `key`, `text`), painting, and system
changes (`dpi`, `display_change`, `theme_change`, `settings_change`).

A window can also take a plain callback and decode the message itself:

```cpp
window.set_event_handler([](const helios::event_message& message) {
    switch (message.kind) {
    case helios::event_kind::resize: {
        helios::resize_event event{};
        std::memcpy(&event, message.data, message.size);
        // ...
        break;
    }
    default: break;
    }
});
```

Beyond the typed events, `base_window` offers the usual window operations —
`show`/`hide`, `set_size`/`set_position`/`center`, `minimize`/`maximize`/
`restore`, `focus`/`activate`, `set_always_on_top`, `set_opacity`, `enable`,
`set_resizable`, `set_min_size`/`set_max_size`, `dpi`/`scale`, and the
`window_options` flags `centered` and `always_on_top`.

Raw Win32 messages (WM_*, WPARAM/LPARAM) stay out of the portable header; they
live in the platform extension header:

```cpp
#include <HeliosView2/Win32.h>

helios::win32::set_native_message_handler(window,
        [](helios::win32::native_message& message) {
            if (message.id == WM_APP + 1) {
                message.handled = true;
                message.result = 0;
            }
        });
```

The scanner needs C++26 reflection, so this example is built with
`-freflection` (GCC 16+).

```powershell
cmake --build build --target window_example
./build/bin/window_example.exe
```

Close the window with its title-bar X; the last window triggers `app::quit()`
and `app::run()` returns.

Non-interactive smoke test:

```powershell
./build/bin/window_example.exe --smoke
```
