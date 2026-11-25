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

### Linux

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install g++ build-essential libgtk-3-dev libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev freeglut3-dev libegl-dev libwayland-dev libavcodec-dev \
    libavformat-dev libavfilter-dev libswscale-dev libsdl2-dev libavutil-dev \
    libportmidi-dev libzstd-dev libwebp-dev libcurl4-openssl-dev \
    libsecret-1-dev libltc-dev cbp2make

# Build (downloads wxWidgets automatically if not installed)
make -j$(nproc)

# Install
sudo make install
```

### Windows

Prebuilt dependencies (wxWidgets, FFmpeg, curl, SDL2, zstd, zlib) come from the same
[xLights-dependencies](https://github.com/xLightsSequencer/xLights-dependencies) bundle xLights uses, pinned by
`xlights/ci_scripts/deps_version.txt`. The projects fetch it automatically (via
`xlights/ci_scripts/fetch_dependencies.ps1`) into `xlights/dependencies-bundle` before compiling. To build against
a local wxWidgets build instead, set the `WXWIDGETS_ROOT` environment variable.

1. Open `xSchedule/xSchedule.sln` in Visual Studio 2022
2. Build Release x64

Or use the build script:
```cmd
cd build_scripts\msw
call build_xSchedule_x64.cmd
```

## Repository Structure

xSchedule's own source code lives in `xSchedule/`. Shared code from the xLights project (outputs, controllers, utilities, audio/video support) is provided via the `xlights/` git submodule.

After cloning, initialize submodules:
```bash
git submodule update --init --recursive
```

## License

xSchedule is licensed under the [GNU General Public License v3.0](LICENSE).
