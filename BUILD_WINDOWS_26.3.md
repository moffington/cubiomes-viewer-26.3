# Rebuild on Windows x64

Use a workspace path without spaces. Install/extract the Qt 6.8.3 MinGW desktop
**qtbase** package and matching MinGW 13.1.0 toolchain into the workspace. No
system-wide installation is needed. MSVC is unsupported by the pinned backend.

Official binary package repository:
https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/qt6_683/qt6_683/

Official compiler package repository:
https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/tools_mingw1310/

Use a private terminal environment with the MinGW `bin` directory followed by
Qt's `bin` directory on PATH. From a separate `build` directory:

```powershell
qmake ..\cubiomes-viewer.pro CONFIG+=release CONFIG-=debug_and_release
mingw32-make -j1
```

If starting from Git, initialize the Cubiomes submodule. It must be the pinned
xpple commit recorded in README-26.3.md; it is already included in the source ZIP.

The qmake project directly compiles the backend's generation sources. It omits
unused loot parsing code and uses a small wrapper to avoid a `util.o` collision.
Do not remove `-fwrapv` or enable `-ffast-math`.

Deploy `Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Widgets.dll`, `libgcc_s_seh-1.dll`,
`libstdc++-6.dll`, and `libwinpthread-1.dll` beside the executable. Copy
`plugins/platforms/qwindows.dll`, `plugins/platforms/qoffscreen.dll` and
`plugins/styles/qmodernwindowsstyle.dll` into corresponding `platforms` and
`styles` folders beside it. Add `qt.conf`:

```ini
[Paths]
Prefix=.
Plugins=.
```

Include the license notices from the delivered portable package. Keep Qt dynamic
so users can replace/relink its libraries.

Run `cubiomes-viewer.exe --self-test` with captured stdout; it enforces Qt's
offscreen platform internally and uses isolated temporary INI settings.
`tests/validate_structures.py` accepts a compiled Cubiomes DLL and runs independent
placement and biome-eligibility checks using the included final-release data.

For the backend's complete existing test suite, configure its CMake project with
MinGW, build with `--parallel 1`, then run `ctest --output-on-failure -j1`. Add
`-fwrapv` to `CMAKE_C_FLAGS_RELEASE` because the backend's Windows CMake code
replaces `CMAKE_C_FLAGS`.
