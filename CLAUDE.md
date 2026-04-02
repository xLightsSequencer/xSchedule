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
Open `xSchedule/xSchedule.sln` in Visual Studio 2022 and build Release x64. wxWidgets must be cloned as a sibling directory (`../../wxWidgets/`).

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
  - `libltc/` — SMPTE timecode headers
  - `Xyzzy/` — configuration/sample data
- **`xlights/`** — git submodule pointing to the xLights repository, providing:
  - `xlights/xLights/outputs/` — all output protocol implementations
  - `xlights/xLights/controllers/` — controller hardware handlers
  - `xlights/xLights/utils/` — shared utilities (UtilFunctions, CurlManager, AudioManager, etc.)
  - `xlights/xLights/render/` — FSEQFile, VideoReader, SequenceData
  - `xlights/xLights/ui/` — shared UI utilities, Discovery
  - `xlights/xLights/vamp-hostsdk/` — Vamp audio analysis plugins
  - `xlights/common/` — base application framework
  - `xlights/include/` — shared headers, icons
  - `xlights/dependencies/` — pugixml, spdlog submodules
- **`bin/`** — xScheduleWeb directory, desktop files
- **`images/icons/`** — application icons for Linux

### Include Path Mapping
xSchedule source files reference shared code via `../xlights/` paths:
- `#include "../xlights/xLights/utils/UtilFunctions.h"`
- `#include "../xlights/common/xlBaseApp.h"`
- `#include "../xlights/xLights/outputs/OutputManager.h"`

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

wxWidgets 3.3 (custom fork `xLightsSequencer/wxWidgets`, branch `xlights_2026.04`), spdlog, libcurl, pugixml, FFmpeg, SDL2, PortMIDI, libltc, zstd, nlohmann/json.
