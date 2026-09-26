# 诗词壁纸（poetry_desktop）

Qt6 / C++17 的 Windows 桌面应用：把本地图片和诗词随机组合，合成带排版文字的壁纸并设为桌面。
产品与设计约束见 `docs/`：`PRODUCT_SPEC.md`（功能与验收）、`REQUIREMENTS_SYNC.md`（当前需求基线）、
`TYPOGRAPHY_SPEC.md`（排版规范）、`UI_REDESIGN.md`（界面改版方案）。

## 依赖

| 组件 | 版本 | 说明 |
|---|---|---|
| Qt | 6.8 以上（实测 6.8.3） | 需要 `Core` `Gui` `Widgets` `Concurrent` `Test` 五个组件 |
| 编译器 | MinGW-w64 GCC 13.1.0（Qt 官方 `Tools/mingw1310_64`） | 必须与 Qt 构建套件匹配 |
| CMake | 3.25 以上 | |
| 生成器 | `MinGW Makefiles` | |

## 关键配置项：CMAKE_PREFIX_PATH

`CMakeLists.txt:11-20` 会在 `CMAKE_PREFIX_PATH` 未设置时，探测一组已知的 Qt 安装位置，
命中条件是该目录下存在 `lib/cmake/Qt6/Qt6Config.cmake`。这个列表是本机路径，换机器不一定命中，
因此**优先级从高到低**有三种正确做法：

1. **在 IDE / 命令行显式传入**（推荐，不动仓库）
   ```bash
   cmake -DCMAKE_PREFIX_PATH=D:/Qt/6.8.3/mingw_64 -G "MinGW Makefiles" -S . -B build
   ```
   CMake 也会自动读取同名环境变量 `CMAKE_PREFIX_PATH`。
2. **改 `CMakeLists.txt:14` 的探测列表**，把你机器上的 Qt 根目录加进去。
3. 什么都不做 —— 只有当列表里恰好有你的路径时才能配过。

## 命令行构建

```bash
export PATH="/d/Qt/Tools/mingw1310_64/bin:$PATH"        # 提供 gcc / g++ / mingw32-make
cmake -DCMAKE_BUILD_TYPE=Release -G "MinGW Makefiles" -S . -B build
cmake --build build -j 10
```

## CLion 配置（首次打开项目必读）

### 1. Toolchain

Settings → Build, Execution, Deployment → **Toolchains** → `+` 新建一个 MinGW 工具链：

| 字段 | 值 |
|---|---|
| C Compiler | `<Qt>/Tools/mingw1310_64/bin/gcc.exe` |
| C++ Compiler | `<Qt>/Tools/mingw1310_64/bin/g++.exe` |
| Build Tool | `<Qt>/Tools/mingw1310_64/bin/mingw32-make.exe` |
| Debugger | 自动探测的 MinGW GDB |

### 2. CMake：指定 Qt 位置

Settings → Build, Execution, Deployment → **CMake** → 选中 profile（如 `Debug`）→
展开 **Cache variables** 折叠行 → 点表格左上角的 `+` → 新增：

```
CMAKE_PREFIX_PATH = <Qt>/6.8.3/mingw_64
```

注意三点：

- **`Cache variables` 是一个折叠区**，行首有 `>`，不点开是看不到 `+` 按钮的。
- 路径用**正斜杠**（`S:/soft/QT/6.8.3/mingw_64`），不要写 `S:\soft\...`。
- 改完必须点 **Apply**，CLion 才会重新 Configure。

如果之前配置失败过，`Qt6_DIR` 会被缓存成 `Qt6_DIR-NOTFOUND`，单纯 Reload 不一定重新查找。
此时执行 **Tools → CMake → Reset Cache and Reload Project**。

### 3. Run / Debug 需要 Qt 的 bin 在 PATH 里

`poetry_desktop.exe` 依赖 `Qt6Core.dll` 等。不配 PATH 的话进程起来就闪退。

Run/Debug Configurations → **Edit Configurations** → 左侧模板 **CMake Application** →
Environment 加：

```
PATH=<Qt>\6.8.3\mingw_64\bin;%PATH%
```

改模板可以一次覆盖 `poetry_desktop`、`poetry_tests`、`poetry_ui_tests` 三个配置。

### 4. 文件编码必须设为 UTF-8

Settings → Editor → **File Encodings** → Global Encoding 与 Project Encoding 都设为 `UTF-8`。

源码里有大量中文字面量（UI 文案、错误提示）。历史上发生过一次非 UTF-8 编辑，
把中文字符写成了字面问号，界面上直接显示 `?`。这一项不是风格偏好，是防回归。

### 5. 建议勾选自动重配

CMake 设置页顶部的 **Reload CMake project on editing CMakeLists.txt or other CMake configuration files**。

## 测试

```bash
cd build && ctest --output-on-failure
```

`CMakeLists.txt:91-99` 已为两个测试目标注入 `PATH`、`QT_QPA_PLATFORM=windows` 和工作目录，
所以走 ctest 不需要额外配置。但**在 CLion 里点代码行号旁的绿色三角直接跑单个测试用例不走 ctest**，
需要靠上面第 3 步的 PATH 模板兜住。

当前基线：`poetry_tests` 17 项、`poetry_ui_tests` 5 项，全部通过。

## 运行时数据位置

应用数据在 `%LOCALAPPDATA%\PoetryDesktop\PoetryDesktop\`：

```
config.json            配置（图片源、诗词、渲染设置、定时、运行态）
poetry_desktop.lock    单实例锁（陈旧锁 30 秒后自动失效）
cache/wallpapers/      合成壁纸输出，保留最近 10 张
logs/app.log           日志，超过 1MB 轮转为 app.log.1
```

`config.json` 损坏时不会丢配置：自动备份为 `config.json.corrupt-<时间戳>` 并回落到默认值。

## 首次运行

全新环境启动后是空配置状态：只有 3 首内置默认诗词（`src/core/libraries.cpp` 的 `PoetryLibrary::defaultPoems()`），
**没有任何图片来源**，顶部状态显示「资源不足」。需要到「图片库」页添加一个图片目录并等待扫描完成，
才能生成预览。

关闭主窗口默认只是隐藏到系统托盘，程序继续运行；退出请用「更多 → 退出」或托盘菜单。

## 分辨率与 DPI 适配

分三层，各自机制不同：

| 层 | 机制 | 1080p / 2K / 4K 表现 |
|---|---|---|
| **输出壁纸尺寸** | `MainWindow::targetWallpaperSize()` = `QScreen::geometry() × devicePixelRatio` | 始终等于屏幕**物理**分辨率：1920×1080 / 2560×1440 / 3840×2160。已实测 4K@125 % 产出恰好 3840×2160 |
| **壁纸上的排版** | `renderer.cpp` 的 `resolutionScale = qBound(0.85, √(画布面积 / 1920×1080), 2.4)`，作用于字号与三处间距；边距与安全区按画布尺寸取比例 | 基准 36 pt 时：1080p→36 pt、1440p→48 pt、4K→71 pt。上限 2.4 倍，约 5K 以上不再增长 |
| **窗口自身 UI** | Qt 6 默认开启高 DPI 缩放 + 控件用**点字号**（基准 12 pt），跟随系统缩放比例 | 随系统缩放自动放大，无需额外适配 |

**窗口最小尺寸**：`900 × 640` 逻辑像素，加上标题栏约 31 px，需要 **~671 px 的逻辑工作区高度**。

| 屏幕 | 系统缩放 | 逻辑工作区高 | 是否放得下 |
|---|---|---|---|
| 1920×1080 | 100 % / 125 % / **150 %** | 1040 / 816 / 682 | ✓ |
| 2560×1440 | 100 %–150 % | ≥ 862 | ✓ |
| 3840×2160 | 100 %–200 % | ≥ 720 | ✓ |
| 1366×768 | 100 % | 728 | ✓ |
| 1366×768 | 125 % | 582 | ✗ **明确不支持** |

最后一条是有意划的边界：1366×768 且 125 % 缩放的机器，逻辑工作区只有 582 px，
再压最小窗口会让预览和设置面板都没法用。这个下限由 `test_ui` 断言保护，
实际可用性由 `settingsControlsNeverOverlapEachOther` 在 **1440×900 / 1280×800 / 1024×700 / 900×640**
四个逻辑尺寸下逐页验证控件互不相交来保证。

注意这验证的是**逻辑尺寸**，不等于跑过了三档物理分辨率；物理分辨率侧的实测证据是
4K@125 % 机器上产出恰好 3840×2160 的壁纸。

## 排版数值以哪份文档为准

见 **`docs/REQUIREMENTS_SYNC.md` 第 0 节** —— 它是渲染数值的唯一权威，
`TYPOGRAPHY_SPEC.md` 的具体像素值已标注为历史草案（其结构规则仍有效）。
改字号、锚点、行距、面板色之前先改那一节。代码侧对应 `src/core/models.h` 的 `RenderSettings` 默认值。

## 图标

`resources/app_icon.ico` 是多尺寸图标，16/20/24 px 用的是简化版（圆角底 + 月牙），
32 px 及以上是完整版（夜空 + 月牙 + 山影 + 诗句线）。改图标时要重新生成整个 .ico，
只换单一尺寸会让小尺寸重新变糊。`resources/app.rc` 负责把它编进 exe 的 PE 资源，
`resources.qrc` 负责给运行时用 —— 两处都要动。

## 故障排查

| 现象 | 原因 / 处理 |
|---|---|
| Configure 报 `Could not find a package configuration file provided by "Qt6"` | `CMAKE_PREFIX_PATH` 没设或指错，见上文「CLion 配置 2」 |
| Cache 里 `Qt6_DIR` 一直是 `Qt6_DIR-NOTFOUND` | Tools → CMake → **Reset Cache and Reload Project** |
| exe 双击/Debug 起来即退出 | Qt 的 `bin` 不在 PATH，见「CLion 配置 3」 |
| 界面中文显示成 `?` | 编辑时用了非 UTF-8 编码，见「CLion 配置 4」 |
| 点单个测试用例报找不到 DLL | gutter 运行不走 ctest，见「测试」一节 |
| 编译随机报 `Fatal error: can't create ...obj: Invalid argument` / `ar.exe: unable to copy file objects.a` | 构建目录并发写入被抢（杀软实时扫描 + IDE 索引，且本项目的构建目录被 git 跟踪）。**改用 `-j 1` 串行构建即可通过**，或在 IDE 里执行 Tools → CMake → Reset Cache and Reload Project 重建目录 |
| 加了测试用例但运行数量没变 | AUTOMOC 可能不重新生成 `*_autogen/include/*.moc`，新槽位不注册且**不报错**。删掉构建目录重来才能看到真实数量 |
| 编译中文变成字面 `?` | 见「CLion 配置 4」，Project Encoding 必须是 UTF-8 |
| 提示「诗词壁纸已经在运行中」 | 存在 `poetry_desktop.lock`；确认无残留进程后删除该文件 |
