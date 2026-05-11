# FCGo — NES 模拟器

C++17 模块化 NES/FC 模拟器，支持 SDL2 / OpenGL 双渲染后端热切换。

## 架构

```
FCGo/
├── src/
│   ├── core/               # 核心模拟层（无平台依赖）
│   │   ├── types.h         # 公共类型 (u8/u16/u32...)
│   │   ├── cpu/            # MOS 6502 CPU（全指令+非官方指令集）
│   │   ├── ppu/            # 2C02 PPU（BG/Sprite/ScrollRegs）
│   │   ├── apu/            # APU（Pulse×2/Triangle/Noise/DMC）
│   │   ├── mapper/         # Mapper 0/1/2/3/4 + 工厂模式
│   │   ├── memory/         # CPU/PPU总线（Bus）
│   │   └── nes_console.h   # 顶层 NESConsole 组合类
│   ├── renderer/           # 渲染抽象层
│   │   ├── irenderer.h     # IRenderer 接口
│   │   ├── sdl_renderer    # SDL2 硬件加速 Texture 渲染
│   │   ├── opengl_renderer # OpenGL 2.1 Quad 渲染
│   │   └── renderer_factory# 工厂 + 运行时热切换
│   ├── audio/              # 音频抽象层
│   │   ├── iaudio_backend.h# IAudioBackend 接口
│   │   └── sdl_audio_backend# SDL2 AudioDevice 回调
│   ├── input/              # 键盘→手柄映射
│   ├── app/                # EmulatorApp 主循环
│   └── main.cpp            # 入口点 + 参数解析
├── tests/
│   └── test_nes.cpp        # 自包含单元测试（无额外依赖）
├── third_party/
│   └── SDL2/               # SDL2 开发库（运行 setup_sdl2.bat 安装）
├── FCGo.sln                # VS2022 解决方案
├── FCGo.vcxproj            # 主程序项目
├── FCGoTest.vcxproj        # 单元测试项目
└── setup_sdl2.bat          # 一键下载安装 SDL2 开发库
```

## 快速开始

### 1. 安装 SDL2 开发库

双击运行 `setup_sdl2.bat`（会自动下载并解压到 `third_party/SDL2/`）

或手动下载：
- https://github.com/libsdl-org/SDL/releases/download/release-2.30.3/SDL2-devel-2.30.3-VC.zip
- 解压后将 `include/` 和 `lib/x64/` 放到 `third_party/SDL2/`

### 2. 用 VS2022 打开并编译

```
打开 FCGo.sln → 选 x64 Debug → Build Solution
```

编译后会自动将 `SDL2.dll` 和 ROM 文件复制到输出目录。

### 3. 运行

```bash
# 直接运行（Debug 模式自动加载 tank.nes）
bin\Debug\FCGo.exe

# 指定 ROM 和渲染器
bin\Debug\FCGo.exe chiyingzhanshi.NES --renderer opengl --scale 3

# 查看帮助
bin\Debug\FCGo.exe --help
```

### 4. 运行单元测试

```
在 VS2022 中将 FCGoTest 设为启动项目 → F5
```

或：
```bash
bin\Debug\FCGoTest.exe
```

## 按键说明

| 按键 | 功能 |
|------|------|
| 方向键 | D-Pad |
| Z | B 键 |
| X | A 键 |
| Enter | Start |
| Shift | Select |
| R | Reset |
| F1 | 切换到 SDL2 渲染器 |
| F2 | 切换到 OpenGL 渲染器 |

## 命令行参数

```
FCGo.exe [options] <rom.nes>
  --renderer <sdl|opengl>   渲染器选择（默认: sdl）
  --scale <1-4>             窗口缩放倍数（默认: 3 = 768x720）
  --help                    显示帮助
```

## 支持的 Mapper

| Mapper | 名称 | 代表游戏 |
|--------|------|---------|
| 0 | NROM | 坦克大战、打鸭子 |
| 1 | MMC1/SxROM | 塞尔达传说、最终幻想 |
| 2 | UxROM | 魂斗罗、洛克人 |
| 3 | CNROM | 恶魔城、忍者神龟 |
| 4 | MMC3/TxROM | 超级马里奥3、双截龙 |

## 扩展渲染器

实现 `IRenderer` 接口，在 `RendererFactory::create()` 中注册即可：

```cpp
class MyVulkanRenderer : public IRenderer {
    bool init(const std::string& title, int w, int h) override { ... }
    void present(const uint32_t* fb, int nesW, int nesH) override { ... }
    bool pollEvents() override { ... }
    bool isKeyDown(int key) const override { ... }
    const char* name() const override { return "Vulkan"; }
};
```

## 扩展音频后端

实现 `IAudioBackend` 接口：

```cpp
class XAudio2Backend : public IAudioBackend {
    bool init(int sampleRate, int channels) override { ... }
    void pushSamples(const std::vector<float>& samples) override { ... }
    // ...
};
```
