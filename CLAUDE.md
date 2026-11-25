# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

xSchedule is a show scheduler and player for lighting control. It plays FSEQ sequences, manages playlists, responds to events, and outputs to controllers via multiple protocols (E1.31, Art-Net, DDP, DMX, etc.). Includes a web UI, plugin system, and REST API.

Built on wxWidgets 3.3 (custom fork).

**Supported platforms:** Linux (Debian 12 / Ubuntu 24.04), Windows 10+.

## Build Commands

### Linux
```bash
make                          # Full build (wxWidgets + xSchedule + plugins)
make debug                    # Debug build
make clean                    # Clean all
# Output binaries go to bin/
```

Build uses Code::Blocks .cbp project files converted to makefiles via cbp2make. Object files go to `.objs_debug/` or `.objs_release/`.

### Windows
Open `xSchedule/xSchedule.sln` in Visual Studio 2022 and build Release x64. Prebuilt dependencies (wxWidgets, FFmpeg, curl, SDL2, zstd, zlib) come from the xLights-dependencies bundle: each project runs `xlights/ci_scripts/fetch_dependencies.ps1 -Only Dependencies` before compiling, staging headers/libs into `xlights/dependencies-bundle/` and DLLs into `xlights/bin64/`. Set `WXWIDGETS_ROOT` to build against a local wxWidgets instead.

Or use the build script:
```cmd
cd build_scripts\msw
call build_xSchedule_x64.cmd
```

### wxSmith Generated Code
Some dialogs use wxSmith (wxWidgets RAD tool). Generated code is delimited by `//(* ... //*)` guards in `.cpp`/`.h` files. **Any changes within these guards MUST also be reflected in the corresponding `.wxs` file** in `xSchedule/wxsmith/`. Otherwise the changes will be overwritten the next time the `.wxs` file is opened in wxSmith.

### Adding New Source Files
When adding new `.cpp`/`.h` files, the following project files must be updated manually:
- **`xSchedule/xSchedule.cbp`** — add `<Unit filename="...">` entries (used by Linux build via cbp2make)
- **`xSchedule/xSchedule.vcxproj`** — add `<ClCompile>` for `.cpp` and `<ClInclude>` for `.h`
- **`xSchedule/xSchedule.vcxproj.filters`** — add corresponding filter entries

## Repository Structure

- **`xSchedule/`** — application source files with subdirectories:
  - `PlayList/` — playlist management (141 files)
  - `events/` — event handling system (52 files)
  - `RemoteFalcon/` — Remote Falcon plugin (builds as DLL/SO)
  - `xSMSDaemon/` — SMS notification plugin (builds as DLL/SO)
  - `wxHTTPServer/` — embedded HTTP server for web UI
  - `wxMIDI/` — wxWidgets MIDI wrapper
  - `wxJSON/` — JSON parsing library
  - `libltc/` — libltc 1.3.2 (SMPTE timecode). Linux links the system `libltc`; Windows compiles these sources into xSchedule (the xLights dependency bundle does not provide it). Same code as the `xLightsSequencer/libltc` fork used previously, which only added Windows build files.
  - `portmidi/` — PortMidi 217 (used by `wxMIDI/`). Linux links the system `libportmidi`; Windows compiles the `pm*`/`pt*` sources into xSchedule with `UNICODE` undefined (217 uses the ANSI MMSystem structs).
  - `Xyzzy/` — configuration/sample data
- **`xlights/`** — git submodule pointing to the xLights repository, providing:
  - `xlights/src-core/outputs/` — all output protocol implementations
  - `xlights/src-core/controllers/` — controller hardware handlers
  - `xlights/src-core/utils/` — shared utilities (UtilFunctions, CurlManager, FileUtils, etc.)
  - `xlights/src-core/media/` — AudioManager, VideoReader, FFmpeg/SDL audio and video backends
  - `xlights/src-core/render/` — FSEQFile, SequenceData
  - `xlights/src-ui-wx/shared/utils/` — shared wxWidgets UI utilities (wxUtilities, xLightsTimer)
  - `xlights/dependencies/vamp-hostsdk/` — Vamp audio analysis plugins
  - `xlights/common/` — base application framework
  - `xlights/include/` — shared headers, icons
  - `xlights/dependencies/` — pugixml, spdlog submodules
- **`controllers/`** — local copies of the `.xcontroller` hardware-definition XML files, bundled by the Windows installer into `{app}/controllers` so a standalone xSchedule install works without xLights also being installed (see note below). Runtime code that reads this folder (`ControllerCaps::LoadControllers()`) lives in `xlights/src-core/controllers/ControllerCaps.cpp`.
- **`bin/`** — xScheduleWeb directory, desktop files
- **`images/icons/`** — application icons for Linux

### Include Path Mapping
xSchedule source files reference shared code via `../xlights/` paths:
- `#include "../xlights/src-core/utils/UtilFunctions.h"`
- `#include "../xlights/common/xlBaseApp.h"`
- `#include "../xlights/src-core/outputs/OutputManager.h"`

### Shared xLights code hooks
xSchedule compiles selected xLights sources directly and must provide a few things the xLights app normally does:
- `xSchedule/xScheduleConfig.cpp` implements `GetXLightsConfig()`/`XLightsConfigAdapter` (declared in `xlights/src-ui-wx/settings/XLightsConfigAdapter.h`) on top of xSchedule's own `wxConfig`, so shared code never writes into xLights' `settings.json`. `ReadXLightsSetting()` reads xLights' own settings (e.g. `LastDir`) from its `settings.json`, falling back to legacy `wxConfig("xLights")`.
- `xScheduleApp::OnInit()` calls `SetMainThreadId()`, `GetResourcesDirectory()` (required by `ControllerCaps` to find `controllers/`), and installs the `AppCallbacks` handlers.
- `xSchedule/DiscoveryStub.cpp` stubs the `Discovery` symbols referenced by some output classes.

## Code Style

- C++20 with GNU extensions (`-std=gnu++20`)
- 4-space indentation, no tabs
- No column limit (ColumnLimit: 0)
- Opening braces on same line (K&R style)

- **Strings**: Use `std::string` instead of `wxString`. Convert at wx API boundaries with `.ToStdString()` / `wxString(str)`.
- **Collections**: Use `std::vector`, `std::map`, etc. instead of `wxArrayString`, `wxList`, etc.
- **Exceptions**: Do NOT use `std::stoi`, `std::stol`, `std::stod` — they throw on invalid input. Use `std::strtol`, `std::strtod` instead.
- **File existence checks**: Use `FileExists()` from `ExternalHooks.h` instead of `std::filesystem::exists()`.

## Key Dependencies

wxWidgets 3.3 (custom fork `xLightsSequencer/wxWidgets`; tag pinned by `xlights/ci_scripts/deps_version.txt`, used by both the Linux Makefile and the Windows dependency bundle), spdlog, libcurl, pugixml, FFmpeg, SDL2, PortMIDI, libltc, zstd, nlohmann/json.

## Maintenance Notes

- **`controllers/` is a hand-maintained duplicate of `xlights/resources/controllers/`.** It was added (2026-08) to fix a standalone-install bug (xSchedule issue #13: installer never packaged a `controllers` folder, so `ControllerCaps::LoadControllers()` logged "Controllers folder not found" and every controller showed disconnected). Deliberately NOT sourced from the `xlights` submodule at install/build time, to avoid depending on upstream xLights changes/PRs for an xSchedule-only bug fix. Consequence: when upstream xLights adds a new controller vendor or edits an existing `.xcontroller` file, this copy will NOT pick it up automatically — periodically diff `controllers/` against `xlights/resources/controllers/` and re-copy as needed, especially before cutting an xSchedule release.
