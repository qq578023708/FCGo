# FCGo — NES Emulator

**English** | [简体中文](README_zh_CN.md)

A C++17 modular NES/FC emulator with SDL2/OpenGL dual rendering backend hot-switching and Steam multiplayer support.

## Features

- **Accurate Emulation**: MOS 6502 CPU (full instruction set), 2C02 PPU, APU audio
- **Dual Rendering Backends**: SDL2 / OpenGL hot switching
- **Steam Multiplayer**: Two-player online battles via Steamworks SDK
- **Debugging Tools**: Built-in debugger, memory viewer, PPU viewer
- **Cross-Platform**: Windows, Linux support

## Quick Start

### Windows (Visual Studio 2022)

```bash
# 1. Install dependencies
# - Visual Studio 2022 (with C++ Desktop Development)
# - Qt 6.x (https://www.qt.io/download)
# - SDL2 (run setup_sdl2.bat for automatic installation)

# 2. Build
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release

# 3. Run
./build/Release/FCGo.exe
```

### Windows (MSYS2/MinGW)

```bash
# 1. Install MSYS2 and dependencies
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
pacman -S mingw-w64-x86_64-qt6-base mingw-w64-x86_64-qt6-tools mingw-w64-x86_64-SDL2

# 2. Build
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 3. Run
./build/FCGo.exe
```

### Linux

```bash
# 1. Install dependencies
sudo apt-get install cmake g++ ninja-build libsdl2-dev qt6-base-dev qt6-tools-dev

# 2. Build
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 3. Run
./build/FCGo
```

## VS Code Development

The project includes complete VS Code configuration supporting both MSVC and MinGW build chains:

| Task | Description |
|------|-------------|
| `CMake Build (MSVC)` | Build with MSVC + Ninja |
| `CMake Build (MinGW)` | Build with MinGW GCC + Ninja |
| `CMake Build Release (MinGW)` | MinGW Release build |

**Debug Configurations**:
- `Debug FCGo (MSVC)` - Visual Studio debugger
- `Debug FCGo (MinGW)` - GDB debugger
- `Debug FCGo (MinGW, with ROM)` - Debug with ROM

Press `F5` to start debugging, `Ctrl+Shift+B` to run the default build task.

## Controls

### Player 1 (P1)

| Key | Function |
|-----|----------|
| W/A/S/D | D-Pad |
| J | B Button |
| K | A Button |
| U | Turbo B |
| I | Turbo A |
| Enter | Start |
| Shift | Select |

### Player 2 (P2)

| Key | Function |
|-----|----------|
| Arrow Keys | D-Pad |
| 0 (Zero) | B Button |
| . (Period) | A Button |
| 1 | Turbo B |
| 2 | Turbo A |
| + | Start |
| - | Select |

### System Keys

| Key | Function |
|-----|----------|
| R | Reset |
| F1 | Switch to SDL2 renderer |
| F2 | Switch to OpenGL renderer |

## Steam Multiplayer

FCGo integrates Steamworks SDK for two-player online battles via Steam.

### Features

- **Steam Friend Multiplayer**: Create/join Steam lobbies to play with friends
- **Real-time Latency Display**: Status bar and player list show network latency
- **Lobby Management**: Host can kick players, cooldown prevents malicious reconnection
- **ROM Synchronization**: Host automatically sends ROM to clients
- **Frame Synchronization**: Input-based frame sync ensures consistent game state

### Usage

1. **Launch Steam**: Ensure Steam client is running
2. **Open Lobby Window**: Menu → Network → Open Lobby Window
3. **Create Lobby**: Click "Create Lobby"
4. **Join Lobby**: Select a lobby and click "Join Lobby"
5. **Load ROM**: Host loads ROM, automatically syncs to client
6. **Start Game**: Online play starts automatically after ROM sync

### Multiplayer Controls

| Player | Controller | Controls |
|--------|------------|----------|
| Host (P1) | Controller 1 | Local P1 keyboard settings |
| Client (P2) | Controller 2 | Local P1 keyboard settings |

### Build Requirements

- Steamworks SDK (place in `third_party/steamworks/`)
- Enable `STEAMWORKS_AVAILABLE` macro in CMake

## Supported Mappers

| Mapper | Name | Representative Games |
|--------|------|---------------------|
| 0 | NROM | Tank Battalion, Duck Hunt |
| 1 | MMC1/SxROM | The Legend of Zelda, Final Fantasy |
| 2 | UxROM | Contra, Mega Man |
| 3 | CNROM | Castlevania, Teenage Mutant Ninja Turtles |
| 4 | MMC3/TxROM | Super Mario Bros. 3, Double Dragon |

## Project Structure

```
FCGo/
├── src/
│   ├── core/           # Core emulation layer (CPU/PPU/APU/Mapper)
│   ├── qt/             # Qt GUI, audio, input, networking
│   └── main.cpp        # Entry point
├── cmake/              # CMake scripts (Qt auto-deployment)
├── .github/workflows/  # GitHub Actions CI/CD
├── .vscode/            # VS Code configuration
└── third_party/        # Third-party libraries (SDL2, Steamworks)
```

## Automated Builds

GitHub Actions automatically builds Windows (MinGW) version:

[![Build](https://github.com/qq578023708/FCGo/actions/workflows/build.yml/badge.svg)](https://github.com/qq578023708/FCGo/actions)

Build artifacts can be downloaded from the [Actions](https://github.com/qq578023708/FCGo/actions) page.

## Extension Development

### Adding a New Mapper

Create a new class in `src/core/mapper/mappers/`, inherit from the `Mapper` base class, and register it in `MapperFactory`.

### Adding a New Renderer

Implement the `IRenderer` interface and register it in `RendererFactory::create()`.

## License

GPL License
