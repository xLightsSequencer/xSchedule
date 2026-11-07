# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

xSchedule is a show scheduler and player for lighting control. It plays FSEQ sequences, manages playlists, responds to events, and outputs to controllers via multiple protocols (E1.31, Art-Net, DDP, DMX, etc.). Includes a web UI, plugin system, and REST API.

Built on wxWidgets 3.3 (custom fork).

**Supported platforms:** Linux (Ubuntu 24.04 or newer; xSchedule follows the xLights minimum, so raise it when xLights does), Windows 10+. No macOS build since xSchedule left the xLights repo (2026-04).

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
Windows builds use the xLights dependency bundle (prebuilt wxWidgets, FFmpeg 8, SDL2, curl), staged in `xlights/dependencies-bundle/` by `xlights/ci_scripts/fetch_dependencies.ps1`. The bundle is built with VS 2026, so the x64 configurations use the **v145** toolset (VS 2026 or its Build Tools); VS 2022 cannot link it.

```cmd
cd build_scripts\msw
call build_xSchedule_x64.cmd     # fetches the bundle + VC++ redistributable, builds xSchedule and both plugins
```

After the bundle is staged, `xSchedule/xSchedule.sln` can be built from Visual Studio (Release x64). The plugins have their own solutions (`xSchedule/RemoteFalcon/`, `xSchedule/xSMSDaemon/`).

LibLTC (`xSchedule/libltc/`, 1.3.2) and PortMidi (`xSchedule/portmidi/`, 217) are compiled from source on Windows; xLights no longer ships those libraries. Linux links the system packages instead.

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
  - `libltc/` — SMPTE timecode (header, plus the 1.3.2 sources compiled on Windows)
  - `portmidi/` — PortMidi 217 (headers, plus the Windows sources)
  - `Xyzzy/` — configuration/sample data
- **`xlights/`** — git submodule pointing to the xLights repository. Shared code is split into a wx-free core and a wx layer:
  - `xlights/src-core/outputs/` — all output protocol implementations, OutputManager
  - `xlights/src-core/controllers/` — controller hardware handlers, ControllerCaps
  - `xlights/src-core/media/` — AudioManager, SDL audio output/input, VideoReader, kiss_fft
  - `xlights/src-core/render/` — FSEQFile, SequenceData
  - `xlights/src-core/utils/` — UtilFunctions, FileUtils, ip_utils, CurlManager, AppCallbacks, etc.
  - `xlights/src-core/settings/` — XLightsSettings (xLights' JSON settings store; see ScheduleConfig below)
  - `xlights/src-core/discovery/` — Discovery
  - `xlights/src-ui-wx/shared/utils/` — wxUtilities, xLightsTimer
  - `xlights/dependencies/vamp-hostsdk/` — Vamp host (only needed because AudioManager references it)
  - `xlights/common/` — base application framework (xlBaseApp)
  - `xlights/include/` — shared headers, icons
  - `xlights/resources/controllers/` — the `.xcontroller` hardware definitions. The Windows installer and `make install` copy them into `controllers/` beside the program, so a standalone xSchedule finds them without xLights installed (xSchedule issue #13). `ControllerCaps::LoadControllers()` (`xlights/src-core/controllers/ControllerCaps.cpp`) reads `FileUtils::GetResourcesDir() + "/controllers"`, which xSchedule sets at startup.
- **`bin/`** — xScheduleWeb directory, desktop files
- **`images/icons/`** — application icons for Linux

### Include Path Mapping
xSchedule source files reference shared code via `../xlights/` paths:
- `#include "../xlights/src-core/utils/UtilFunctions.h"`
- `#include "../xlights/common/xlBaseApp.h"`
- `#include "../xlights/src-core/outputs/OutputManager.h"`
- `#include "../xlights/src-ui-wx/shared/utils/wxUtilities.h"`

### Hooks the xLights core needs from xSchedule
The core is wx-free and reaches the app through hooks registered in `xScheduleApp::OnInit`. Without them it still compiles but behaves differently:
- `AppCallbacks` (main-thread posting, error dialogs, crash handling). Without them, "main thread" work runs on the calling thread and errors are only logged.
- `GetResourcesDirectory()` sets the folder the core finds `controllers/` in.
- **Settings:** xSchedule keeps its settings in `settings.json` in `%APPDATA%\xSchedule` (`~/.config/xSchedule` on Linux, `~/Library/Application Support/xSchedule` on macOS). `ScheduleConfig::Initialize()` points the xLights settings store at that file, importing the old registry key (`HKCU\Software\xSchedule`, `~/.xSchedule` on Linux) the first time and leaving it in place for older versions. Read and write settings with `GetXLightsConfig()` (or `ScheduleConfig`), and `Flush()` after writing; the shared helpers (`IsDarkMode`, `GetConfigBool`, ...) read the same store.
- **Plugin settings** are in their own files beside it (`RemoteFalcon.json`, `xSMSDaemon.json`), through `PluginSettings` in `SettingsFiles.h`. Plugins must not point `XLightsSettings` at a file: on Linux the app links with `-rdynamic`, so a plugin's `XLightsSettings` is xSchedule's own. A plugin reads xSchedule's settings with `ReadXScheduleSetting()`.
- **xLights' own settings** (its show folder, media folders) are read with `ScheduleConfig::GetXLightsSetting()` from xLights' `settings.json`, falling back to the registry for older xLights.
- `ScheduleManager::StartOutputToLights()` wraps `OutputManager::StartOutput()`: a failed output is skipped and the rest keep going, without dialogs. Use it instead of calling `StartOutput()` directly.

### Web UI (`bin/xScheduleWeb/`)
Plain jQuery + Bootstrap 3 pages, served as-is (no build step, no CDN; shows often run without internet). Third-party plugins in `Plugins/<name>/<name>.html` are loaded into `index.html`, so keep what they rely on:
- jQuery, Bootstrap 3 CSS/JS, bootstrap-notify, bootstrap-colorpicker, DataTables and the ReconnectingWebSocket script stay loaded.
- Globals: `socket`, `playingStatus`, `uiSettings`, `runCommand`, `notification`, `registerStatusFunction`, `getQueryVariable`, `updatePage`, `storeKey`/`retrieveKey`, `populateSideBar`.
- Element IDs `#nav`, `#header` and `#footer` (plugins hide them for `?args=noheader`), `#pageContent`, and a page-supplied `#help` modal.
- The `uiSettings` stash keeps its keys; add new ones beside them.
- URLs `index.html?page=…&args=…`, `index.html?plugin=…`, `guestbuttons.html` (the server fills in `!PASS!`) and `login.html`.

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

wxWidgets 3.3 (custom fork `xLightsSequencer/wxWidgets`, the tag named in `xlights/ci_scripts/deps_version.txt`, matching the dependency bundle; the Makefile and CI read it from there), spdlog, libcurl, pugixml, FFmpeg (8 on Windows via the bundle, the system version on Linux), SDL2, PortMIDI, libltc, zstd, nlohmann/json.

## Maintenance Notes

- **Following xLights:** the `xlights` submodule is pinned to one xLights commit, normally a release. Moving it is the only step needed to follow xLights: the Windows dependency bundle, the Linux wxWidgets tag and the bundled controller definitions all come from the submodule. A scheduled workflow opens a pull request when xLights tags a new release; merge it once its build passes.
