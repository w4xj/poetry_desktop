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

当前基线：`poetry_tests` 32 项、`poetry_ui_tests` 9 项，全部通过。

## 运行时数据位置

应用数据在 `%LOCALAPPDATA%\PoetryDesktop\PoetryDesktop\`：

```
config.json            配置（图片源、诗词、渲染设置、定时、历史/收藏/排除、自启、运行态）
samples/               首启向导复制的示例图与「当前桌面壁纸」
poetry_desktop.lock    单实例锁（陈旧锁 30 秒后自动失效）
cache/wallpapers/      合成壁纸输出，保留最近 10 张
logs/app.log           日志，超过 1MB 轮转为 app.log.1
```

`config.json` 损坏时不会丢配置：自动备份为 `config.json.corrupt-<时间戳>` 并回落到默认值。

## 首次运行

全新环境启动后会先弹出两页「首次使用引导」：

1. 介绍应用能做什么；
2. 选择底图 —— 内置 3 张示例图（推荐）、当前桌面壁纸、自己的图片文件夹，或跳过。

选内置示例图时会从 qrc 复制到应用数据目录 `samples/`，不改动原图。完成后若已有可用图片，
程序会等扫描结束自动生成并应用第一张壁纸；跳过的用户可随时到「图片库」添加。
之后仍可从「更多 → 首次使用引导」重新打开向导，但重开只添加图片，不会自动换壁纸。

新配置内置 8 首示例诗词（`PoetryLibrary::defaultPoems()`），其中一首是赠答双诗。

关闭主窗口默认只是隐藏到系统托盘，程序继续运行；退出请用「更多 → 退出」或托盘菜单。

## 历史、收藏与排除

「历史」页保留最近 20 次成功应用的「诗词 + 底图」搭配：

- **重新应用** —— 用当前字体/颜色/面板设置重新渲染，不依赖可能已被清理的缓存。
- **收藏** —— 最多 50 条，与 20 条历史分开保存，不会被裁掉；随机切换有 1/4 概率回放收藏搭配。
- **排除底图** —— 只是不参与随机切换，原文件不动，手动预览仍可使用；图片库页可恢复。

「图片库」页的「排除选中图片」按钮是同一份排除列表的快捷入口。

## 导出与复制

预览操作栏的「导出」菜单提供：

- **导出图片…** —— 保存当前成品为 PNG 或 JPG（按文件扩展名决定），采用原子写入。
- **复制图片到剪贴板** —— 直接粘贴到聊天工具或文档。

导出不会改变桌面，也不会清空预览。

## 开机自启

「定时与高级」页勾选「登录 Windows 后自动启动」。实现写当前用户的
`HKCU\Software\Microsoft\Windows\CurrentVersion\Run`，无需管理员权限；
启动命令行带 `--autostart`，登录后静默驻留托盘。每次正常启动都会重写该启动项，
程序目录移动后不会留下失效路径。

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

## 内存与渲染开销（实测）

`PRODUCT_SPEC` 要求"峰值内存 ≤200 MB、连续切换 100 次不泄漏"。这两条已实测，结论是**都满足**，
且过程中纠正了一个误判。

测量手段：`CoreTests::renderingRepeatedlyDoesNotGrowUnbounded` 用
`K32GetProcessMemoryInfo` 采样进程工作集，连做 40 次 3840×2160 合成。

| 指标 | 实测值 |
|---|---|
| 40 次 4K 渲染期间的工作集 | **25 MB，全程零增长**（第 10/20/30/40 次采样完全相同） |
| 渲染路径峰值工作集 | 101 MB |
| 真实运行中的实例（已缓存一张 4K 预览） | 工作集 105 MB / 峰值 174 MB / **私有内存 63 MB** |

**关于"242 MB"的更正**：早前用 `tasklist` 观察到 242 MB 并怀疑泄漏，那个数字是**工作集峰值**，
其中包含读写 2.7 MB PNG 时产生的文件映射缓存页 —— 那部分操作系统在内存紧张时可以直接回收，
不属于进程的私有提交内存。真正的私有内存是 63 MB。判泄漏要看增长趋势，不看工作集绝对值，
所以才有上面那个"连做 40 次、每 10 次采样"的测试：曲线是平的。

顺带在渲染路径上清掉两处纯浪费（`renderer.cpp` 的 `prepareBackground` 先 `convertToFormat`
再 `scaled`，等于多分配一份全尺寸源图；`wallpaper_service.cpp` 合成完又把成品 PNG **整图解码一次**
只为核对尺寸）。峰值内存没变（分配器本来就复用），但**同样的 40 次 4K 渲染耗时从 25.4 s 降到 18.5 s**，
各跑三次区间不重叠，约快 27%。尺寸校验改为只读文件头，`QSaveFile::commit()` 已经保证写入完整。

## 诗词体裁：单首与双诗

一首条目可以是单首，也可以是**由两首组成**的酬答类体裁 —— 一赠一酬、原唱奉和、一问一答。

编辑对话框的「体裁」下拉提供四种：单首 / 赠答 / 唱和 / 问答。选后三种时对话框出现「下首」分组，
壁纸会分上下两块渲染，每块顶部带关系标签（赠·酬、原唱·奉和、问·答），块间距由
「显示设置 → 版面 → 双诗间隔」控制，标签可在同处关闭。

面板形式有两种，同在「显示设置 → 版面 → 双诗面板」切换：

- **合并为一个面板** —— 两块共用一张底板（默认）
- **各自独立成面板** —— 两张卡，中间透出壁纸背景

独立模式下块间距会自动加上两份面板内边距，否则两张卡会互相重叠。

数据上，`Poem` 的下首四个字段与 `relation` 都是**可选键**：旧 `config.json` 直接读进来就是单首，
不需要迁移；把体裁改回「单首」保存时会自动清空下首，不会留下看不见的半首。
导入的 JSON 同样支持这些可选键：

```json
{ "poems": [ {
  "title": "雷神短歌", "author": "柿本人麻吕", "content": "隐约雷鸣，阴霾天空，但盼风雨来，能留你在此。",
  "secondTitle": "和风答章", "secondAuthor": "大伴家持", "secondContent": "隐约雷鸣，阴霾天空，即使天无雨，我亦留此地。",
  "relation": "qa"
} ] }
```

折行遵守中文**避头尾**规则：收尾标点不会成为行首孤字，起首标点不会留在行尾。

## 打包发布

两种产物都自包含，目标机器不需要装 Qt：

- **绿色版** `poetry_desktop-<版本>-windows-x64.zip`：解压后双击 `poetry_desktop.exe`。
- **安装版** `poetry_desktop-<版本>-windows-x64-setup.exe`：NSIS 安装程序，按当前用户安装到
  `%LOCALAPPDATA%\Programs\PoetryDesktop`，不需要管理员权限；创建开始菜单与桌面快捷方式，
  并从 Windows「应用和功能」注册卸载项。卸载不会删除
  `%LOCALAPPDATA%\PoetryDesktop\PoetryDesktop` 里的配置、示例图与缓存。

```bash
export PATH="<Qt>/Tools/mingw1310_64/bin:<Qt>/6.8.3/mingw_64/bin:$PATH"
export PATH="/c/Program Files (x86)/NSIS:$PATH"       # 仅安装版需要 makensis
cmake -DCMAKE_BUILD_TYPE=Release -G "MinGW Makefiles" -S . -B build/release
cmake --build build/release --target poetry_desktop -j 10

cd build/release && cpack -G ZIP                      # 绿色版
cmake --build build/release --target installer -j 10  # 安装版
```

NSIS 可用 `winget install NSIS.NSIS` 安装；`makensis.exe` 不在 PATH 时会跳过 `installer` 目标
（配置阶段给出提示），ZIP 仍可正常打。

### 机制

- 两个产物共用同一套 `install()` 规则和同一份 `windeployqt` 暂存树，避免两套文件清单漂移。
- `windeployqt` 通过 `install(CODE)` 在**安装阶段**执行。必须如此：它要分析的是暂存目录里的那个 exe。
  ZIP 由 CPack 先 install 到临时前缀再压缩；安装版由 `installer` 目标执行
  `cmake --install --prefix build/release/installer-stage`，再调用 `packaging/installer.nsi`。
  `windeployqt` 失败会以 `FATAL_ERROR` 中断，不会静默产出缺 DLL 的包。
- **为什么安装版不用 CPack 的 NSIS 生成器**：它的模板会把非管理员的默认安装目录改到
  「文档」下（`$DOCUMENTS`），与本项目按用户、无提权的定位不符。`packaging/installer.nsi`
  显式装到 `$LOCALAPPDATA\Programs\PoetryDesktop`，`RequestExecutionLevel user`，
  与程序自身写 HKCU 开机自启的权限模型一致；目录页仍允许用户改路径。
- 包名里的版本号来自 `project(poetry_desktop VERSION ...)`。**改版本只改 `CMakeLists.txt` 第 2 行**，
  ZIP、安装包与安装器版本信息自动跟随。
- 包内附带的用户说明是 `packaging/README.txt`（按钮语义、数据位置、支持边界），与开发者读的这份 README 分开维护。
- Qt 的 `Concurrent` 在 MinGW 下是静态库，所以包里**没有** `Qt6Concurrent.dll`，这是正常的，不是漏打。

### CLion 里怎么打

1. External Tool 打绿色版：Program 填 `<CLion>/bin/cmake/win/x64/bin/cpack.exe`，
   Arguments 填 `-G ZIP`，Working directory 填 `$ProjectFileDir$/build/release`。
2. 安装版在 CMake 面板的 target 列表里直接构建 `installer`，或在 IDE 内置终端跑上面两条命令。

### 每次打包后的验收标准

包必须能在**没装 Qt 的机器**上跑。等价的本地验证是把 PATH 换成只剩系统目录再启动：

```bash
cd build/verify/poetry_desktop-*-windows-x64      # 先解压
PATH="C:/Windows/System32;C:/Windows" ./poetry_desktop.exe
```

判据三条：进程持续存活不闪退、没有缺 DLL 弹窗、
`tasklist /FI "PID eq <pid>" /M` 能看到它从**自己所在目录**加载了 `Qt6Core/Qt6Gui/Qt6Widgets`。

安装版可以用静默参数验证。因为按用户安装，不需要提权：

```bash
./poetry_desktop-0.2.0-windows-x64-setup.exe /S /D=S:\temp\PoetryDesktop-test
# 检查安装目录、开始菜单/桌面快捷方式与「应用和功能」条目后卸载
"S:\temp\PoetryDesktop-test\Uninstall.exe" /S
```

实测（2026-10-20）：ZIP 包 24 MB、30 个条目；安装包 18 MB，静默安装到自定义目录后
文件、快捷方式与卸载注册项齐全，净化 PATH 下启动后进程持续存活，卸载后目录和注册项均被清除。

### 已知缺项

已有绿色版和按用户安装的 NSIS 安装包，**没有代码签名**，首次运行 Windows 会弹 SmartScreen 提示；
也没有 MSI 企业部署包。这些属于后续版本范围（见 `PRODUCT_SPEC.md`）。

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
