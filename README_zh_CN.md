# FCGo — NES 模拟器

[English](README.md) | **简体中文**

C++17 模块化 NES/FC 模拟器，支持 SDL2 / OpenGL 双渲染后端热切换，支持 Steam 联机对战。

## 功能特性

- **精确模拟**：MOS 6502 CPU（全指令集）、2C02 PPU、APU 音频
- **双渲染后端**：SDL2 / OpenGL 热切换
- **Steam 联机**：基于 Steamworks SDK 的双人对战
- **调试工具**：内置调试器、内存查看器、PPU 查看器
- **跨平台**：Windows、Linux 支持

## 快速开始

### Windows (Visual Studio 2022)

```bash
# 1. 安装依赖
# - Visual Studio 2022 (包含 C++ 桌面开发)
# - Qt 6.x (https://www.qt.io/download)
# - SDL2 (运行 setup_sdl2.bat 自动安装)

# 2. 构建
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release

# 3. 运行
./build/Release/FCGo.exe
```

### Windows (MSYS2/MinGW)

```bash
# 1. 安装 MSYS2 和依赖
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
pacman -S mingw-w64-x86_64-qt6-base mingw-w64-x86_64-qt6-tools mingw-w64-x86_64-SDL2

# 2. 构建
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 3. 运行
./build/FCGo.exe
```

### Linux

```bash
# 1. 安装依赖
sudo apt-get install cmake g++ ninja-build libsdl2-dev qt6-base-dev qt6-tools-dev

# 2. 构建
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 3. 运行
./build/FCGo
```

## VS Code 开发

项目包含完整的 VS Code 配置，支持 MSVC 和 MinGW 双构建链：

| 任务 | 说明 |
|------|------|
| `CMake Build (MSVC)` | 使用 MSVC + Ninja 构建 |
| `CMake Build (MinGW)` | 使用 MinGW GCC + Ninja 构建 |
| `CMake Build Release (MinGW)` | MinGW Release 构建 |

**调试配置**：
- `Debug FCGo (MSVC)` - Visual Studio 调试器
- `Debug FCGo (MinGW)` - GDB 调试器
- `Debug FCGo (MinGW, with ROM)` - 带 ROM 调试

按 `F5` 开始调试，`Ctrl+Shift+B` 运行默认构建任务。

## 按键说明

### 玩家 1 (P1)

| 按键 | 功能 |
|------|------|
| W/A/S/D | 方向键 (D-Pad) |
| J | B 键 |
| K | A 键 |
| U | B 连发键 (Turbo B) |
| I | A 连发键 (Turbo A) |
| Enter | Start |
| Shift | Select |

### 玩家 2 (P2)

| 按键 | 功能 |
|------|------|
| 方向键 | 方向键 (D-Pad) |
| 0 (数字零) | B 键 |
| . (句号) | A 键 |
| 1 | B 连发键 (Turbo B) |
| 2 | A 连发键 (Turbo A) |
| + | Start |
| - | Select |

### 系统按键

| 按键 | 功能 |
|------|------|
| R | 重置 (Reset) |
| F1 | 切换到 SDL2 渲染器 |
| F2 | 切换到 OpenGL 渲染器 |

## Steam 联机对战

FCGo 集成 Steamworks SDK，支持通过 Steam 进行双人联机对战。

### 功能特性

- **Steam 好友联机**：创建/加入 Steam 大厅，与好友一起游戏
- **实时延迟显示**：状态栏和玩家列表实时显示网络延迟
- **房间管理**：主机可踢出玩家，支持冷却时间防恶意重连
- **ROM 同步**：主机自动将 ROM 发送给客户端，无需手动传输
- **帧同步**：基于输入的帧同步，确保双方游戏状态一致

### 使用方法

1. **启动 Steam**：确保 Steam 客户端正在运行
2. **打开联机窗口**：菜单 → Network → Open Lobby Window
3. **创建房间**：点击 "Create Lobby"
4. **加入房间**：选择房间点击 "Join Lobby"
5. **加载 ROM**：主机加载 ROM 后自动同步给客户端
6. **开始游戏**：ROM 同步完成后自动开始联机

### 联机按键映射

| 玩家 | 控制器 | 按键 |
|------|--------|------|
| 主机 (P1) | Controller 1 | 本地 P1 键盘设置 |
| 客户端 (P2) | Controller 2 | 本地 P1 键盘设置 |

### 编译要求

- Steamworks SDK（放置于 `third_party/steamworks/`）
- CMake 中启用 `STEAMWORKS_AVAILABLE` 宏

## 支持的 Mapper

| Mapper | 名称 | 代表游戏 |
|--------|------|---------|
| 0 | NROM | 坦克大战、打鸭子 |
| 1 | MMC1/SxROM | 塞尔达传说、最终幻想 |
| 2 | UxROM | 魂斗罗、洛克人 |
| 3 | CNROM | 恶魔城、忍者神龟 |
| 4 | MMC3/TxROM | 超级马里奥3、双截龙 |

## 项目结构

```
FCGo/
├── src/
│   ├── core/           # 核心模拟层（CPU/PPU/APU/Mapper）
│   ├── qt/             # Qt GUI、音频、输入、网络
│   └── main.cpp        # 入口点
├── cmake/              # CMake 脚本（Qt 自动部署）
├── .github/workflows/  # GitHub Actions CI/CD
├── .vscode/            # VS Code 配置
└── third_party/        # 第三方库（SDL2、Steamworks）
```

## 自动构建

GitHub Actions 自动构建 Windows (MinGW) 版本：

[![Build](https://github.com/qq578023708/FCGo/actions/workflows/build.yml/badge.svg)](https://github.com/qq578023708/FCGo/actions)

构建产物可在 [Actions](https://github.com/qq578023708/FCGo/actions) 页面下载。

## 扩展开发

### 添加新 Mapper

在 `src/core/mapper/mappers/` 创建新类，继承 `Mapper` 基类，在 `MapperFactory` 中注册。

### 添加新渲染器

实现 `IRenderer` 接口，在 `RendererFactory::create()` 中注册。

## 许可证

GPL License
