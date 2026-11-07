# xSchedule

xSchedule is a show scheduler and player for lighting control. It plays FSEQ sequences, manages playlists, responds to events (MIDI, serial, timecode, API), and outputs to controllers via E1.31 (sACN), Art-Net, DDP, DMX, and other protocols. It includes a built-in web UI for remote control.

xSchedule is part of the [xLights](https://github.com/xLightsSequencer/xLights) family of tools for holiday and entertainment lighting.

## Features

- Play FSEQ sequence files with frame-accurate timing
- Playlist management with scheduling (time-of-day, day-of-week)
- Output to all xLights-supported controllers and protocols
- Built-in web server for remote control (xScheduleWeb)
- Event system: MIDI, serial, E1.31, Art-Net, OSC, FPP Remote, timecode (SMPTE/LTC)
- Video playback support
- Plugin system (xSMSDaemon for SMS control, RemoteFalcon integration)
- REST API for automation

## Building

Clone with the submodules, or initialize them after cloning; the build takes the xLights code and the matching
library versions from them:
```bash
git clone --recursive https://github.com/xLightsSequencer/xSchedule.git
# or, in an existing clone
git submodule update --init --recursive
```

### Linux

```bash
# Install dependencies (Ubuntu 24.04 or newer, the same minimum as current xLights)
sudo apt-get install g++ build-essential pkg-config libgtk-3-dev libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev freeglut3-dev libavcodec-dev \
    libavformat-dev libavfilter-dev libswscale-dev libswresample-dev libsdl2-dev libavutil-dev \
    libportmidi-dev libzstd-dev libwebp-dev libcurl4-openssl-dev libexpat1-dev \
    libsecret-1-dev libltc-dev cbp2make

# Build (downloads wxWidgets automatically if not installed)
make -j$(nproc)

# Install
sudo make install
```

### Windows

Windows builds use the prebuilt libraries (wxWidgets, FFmpeg, SDL2, curl) from the xLights dependency
bundle. The bundle is built with Visual Studio 2026, so the build needs the VS 2026 (v145) toolset; any
edition works, including the free Build Tools.

The build script fetches the bundle and the matching VC++ redistributable, then builds xSchedule and both plugins:
```cmd
cd build_scripts\msw
call build_xSchedule_x64.cmd
```

To work in Visual Studio, run the script once (or `xlights\ci_scripts\fetch_dependencies.ps1 -Only Dependencies`),
then open `xSchedule/xSchedule.sln` and build Release x64.

### GitHub Actions

- The Windows jobs run on the `windows-2025-vs2026` runner image and use the same build script.
- **Follow xLights releases** (`xlights_update.yml`) checks weekly for a new xLights release and opens a pull request
  moving the `xlights` submodule to it. Merge it once its builds pass. It needs **Settings > Actions > General >
  Allow GitHub Actions to create and approve pull requests** turned on. Pull requests opened by a workflow don't
  start other workflows, so it starts Windows CI and Ubuntu CI on its branch itself.

## Repository Structure

xSchedule's own source code lives in `xSchedule/`. Shared code from the xLights project (outputs, controllers, utilities, audio/video support) is provided via the `xlights/` git submodule.

## License

xSchedule is licensed under the [GNU General Public License v3.0](LICENSE).
