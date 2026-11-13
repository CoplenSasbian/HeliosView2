# Window example

Minimal entry point for the Win32 window backend.

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
