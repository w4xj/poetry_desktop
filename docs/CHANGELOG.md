# 变更说明

## 2026-09-26 — 内存调查：没有泄漏，但纠正了我自己的误判，并提速 27%

**起因**：我先前用 `tasklist` 看到进程涨到 242 MB，怀疑泄漏，还把它列为"规范 P0 判据未过"。

**实测结论：没有泄漏，那条 P0 判据本来就是满足的。** 新增
`CoreTests::renderingRepeatedlyDoesNotGrowUnbounded`，用 `K32GetProcessMemoryInfo`
在 40 次 3840×2160 合成过程中每 10 次采样一次工作集 —— **25 MB，四次采样完全相同，零增长**。
真实运行实例的**私有内存只有 63 MB**。

**242 MB 是怎么来的**：`tasklist` 报的是**工作集**，其中包含读写 2.7 MB PNG 时产生的文件映射缓存页，
那部分 OS 在内存紧张时可直接回收，不是进程私有提交内存。判泄漏要看增长趋势而不是工作集绝对值。
这是我这次会话里第二个"用错指标导致误判"的例子，记下来。

**顺带清掉两处真实浪费**（不是为内存，是因为它们本来就是白做的功）：

- `renderer.cpp::prepareBackground` 先 `convertToFormat(ARGB32_Premultiplied)` 再 `scaled()`，
  等于多分配一整份全尺寸源图；`scaled()` 自己就会产出需要的格式
- `wallpaper_service.cpp::renderPreview` 合成完之后又 `QImage::load()` **整图解码**成品 PNG，
  只为核对尺寸；改为 `QImageReader::size()` 只读文件头。`QSaveFile::commit()` 已经保证写入完整，
  整图解码防的是"提交成功但落盘损坏"，超出本应用职责

**A/B 实测**（同一测试各跑 3 次，同机）：

| | 耗时 |
|---|---|
| 改动前 | 26.82 / 24.36 / 25.42 s → 中位 **25.4 s** |
| 改动后 | 21.22 / 18.51 / 18.54 s → 中位 **18.5 s** |

约快 **27%**，两组区间不重叠。**峰值内存没变**（104 → 101 MB，属噪声），因为分配器本来就在复用 ——
所以这条的收益是速度，不是内存，不夸大成内存优化。

**未做的优化**：按目标尺寸解码（`QImageReader::setScaledSize`）只对**比屏幕还大的照片**有效，
而当前图片库里的图都小于 4K 目标，收益为零；且它与 EXIF 旋转（`setAutoTransform`）交互会改变
宽高轴向，处理不当会拉变形。故仅记为候选，不盲目实施。

**验证**：`poetry_tests` 22 项、`poetry_ui_tests` 7 项全过。

---

## 2026-09-26 — 可分发的桌面应用包

新增 CPack 打包链路，产出 23.5 MB 的自包含绿色版 zip（30 个条目），目标机器无需安装 Qt。

**选型**：用 CMake 原生 `install()` + `CPack`，而不是另写一个 `package.bat`。
理由是版本号、构建目录、压缩格式都由 CMake 单一来源决定，脚本方案必然要重新解析一遍
`CMakeLists.txt` 才能拿到版本号 —— 那就是第二份真相。

`windeployqt` 通过 `install(CODE)` 挂在**安装阶段**：它必须分析暂存目录里的 exe，
而 CPack 的流程正是"先 install 到临时前缀，再把该前缀压成包"，所以挂在这里是唯一正确的位置。
执行失败以 `FATAL_ERROR` 中断，不会静默产出缺 DLL 的包。

同时新增 `packaging/README.txt` 作为**面向最终用户**的包内说明（按钮语义、数据位置、
支持边界、如何退出），与面向开发者的 `README.md` 分开维护 —— 两者读者不同，合并会互相拖累。
该文件不写死版本号，版本由目录名承载。

**验收**：按 README 里定的标准实测过 —— 解压后用只剩系统目录的 PATH 启动，
进程存活、无缺 DLL 报错，`tasklist /M` 确认它从自己目录加载了包内的
`Qt6Core/Qt6Gui/Qt6Svg/Qt6Widgets`（共 81 个模块）。
`Qt6Concurrent.dll` 不在包里是正确的，Qt6 的 Concurrent 在 MinGW 下是静态库。

**仍是缺项**：没有安装程序（MSI/NSIS）、没有开机自启、没有代码签名，首次运行会有 SmartScreen 提示。

---

## 2026-09-26 — 仓库清理、窗口下限、排版数值裁定落地

### 构建产物脱离版本控制

1164 / 1203 个跟踪文件是 `cmake-build-debug`、`cmake-build-release`、`qa-build` 的构建输出，
每编译一次就产生 280+ 条待提交变更，真实 diff 被淹没。旧的 `.gitignore` 只写了前两个目录，
而且 **gitignore 对已经进索引的文件无效** —— 必须显式 `git rm -r --cached`。

处理后跟踪文件数 **1203 → 39**，工作区噪音 283 条 → 2 条（只剩 `.idea` 本地状态）。
文件全部保留在磁盘上，只脱离跟踪。同时补了 `/qa-build/` 与 `/build/`。

**意外收益**：清理后并发构建 `-j 10` 一次通过，而之前反复随机报
`Fatal error: can't create ...obj: Invalid argument`（每次不同文件），同样的代码在别的盘一次就过。
确认元凶是构建目录被 git 跟踪后，VCS 文件监视与杀软在抢文件创建锁。这条已记进 README 故障排查。

### 窗口最小尺寸 980×680 → 900×640

原下限需要 ~711 px 逻辑工作区高度，`1920×1080 @150%`（682）放不下。降到 900×640 后需要 ~671 px，
`1080p @150%` 和 `1366×768 @100%` 都能用。之所以敢压，是因为上一轮给设置页加了 `QScrollArea`，
纵向空间变紧时内容会滚动而不是叠印。

`1366×768 @125%`（逻辑工作区 582）**明确划为不支持** —— 再压窗口会让预览和面板都没法用，
这是有意的产品边界，已写进 README 的分辨率表。

`test_ui` 里那条 `minimumSize >= 980×680` 断言改为 `>= 860 / >= 600` 的宽松下限，
真正的可用性保证改由重叠测试承担：现在在 **1440×900 / 1280×800 / 1024×700 / 900×640**
四个逻辑尺寸下逐页验证控件互不相交，四个尺寸全部通过。

### 排版数值裁定：以 REQUIREMENTS_SYNC 为准

四份文档在字号（28 / 32 / 36）、默认锚点（左下 / 右下）、行距（1.35 / 1.60）、面板色（0.42 / 0.6）
上互相矛盾，谁改都能说自己对。裁定写入 **`docs/REQUIREMENTS_SYNC.md` 第 0 节**，理由：
它是唯一标记 *agreed for implementation and product acceptance* 的文档，且日期最新。

`TYPOGRAPHY_SPEC.md` 与 `PRODUCT_SPEC.md` 顶部各加了指向该节的优先级声明，
明确"数值以那份为准、结构规则仍然有效"，避免下次又吵一轮。代码侧 `RenderSettings` 默认值即那张表。

### 朝代独立颜色（采纳 TYPOGRAPHY_SPEC 的结构规则）

之前作者与朝代共用 `metadataColor`，落款"李白 · 唐"里两段同色，层级塌了。
新增 `RenderSettings::dynastyColor`（默认 `#CDCDCD` α168，比作者的 α220 淡一档）：

- `config_store.cpp` 读写该键；旧配置缺键时落到结构体默认值（会略微变淡，是有意的视觉修正）
- `renderer.cpp` 落款的朝代段改用它
- 「显示设置 → 颜色」由 3 个横排改为 **2×2 网格**：标题 / 作者 / 朝代 / 正文

新增测试 `rendererSeparatesAuthorAndDynastyColors`：把标题与正文设成与背景同色的纯黑、
作者纯红、朝代纯蓝，断言两种颜色的像素都大于 0。共用颜色时蓝像素恒为 0，该用例必失败。
这也是上一轮记录"无法为间隔号写断言"的前置条件 —— 颜色分离之后才可测。

**验证**：`poetry_tests` 21 项、`poetry_ui_tests` 7 项全过；并发 `-j 10` 构建无 error / 无 warning。

---

## 2026-09-26 — 应用图标，以及把 qrc 里的死资源清掉

### 图标

新增 `resources/app_icon.ico`（深蓝夜空圆角块 + 月牙 + 山影 + 三条诗句横线），三处全覆盖：

| 位置 | 实现 |
|---|---|
| 标题栏 / 任务栏（运行时） | `main.cpp` 里 `QApplication::setWindowIcon(QIcon(":/app_icon.ico"))` |
| 右下角托盘 | `setupTray()` 改用同一个图标，替换掉原来的 `SP_ComputerIcon`（一台电脑） |
| 资源管理器 / 固定到任务栏 | `resources/app.rc` 把 .ico 编进 PE 的 `.rsrc`，`CMakeLists.txt` 仅在 WIN32 加入该源文件 |

**小尺寸单独做了简化版。** 完整构图在 16 px 下会糊成一团（月亮、山、诗句线挤在一起），
而托盘正是 16 px 场景。所以 .ico 里 16/20/24 用「只有圆角底 + 月牙」的简化版，
32 及以上用完整版。手工装配 ICO 而不是让 PIL 自动缩放，就是为了按尺寸分派不同原画。

验证：`ExtractIconExW` 对 `poetry_desktop.exe` 返回组图标数 1（对照组 `Qt6Core.dll` 为 0），
确认图标真的进了 PE 资源，而不只是 qrc 里有个文件。

### 顺带清掉的死资源

`resources.qrc` 里原本打包了 `default_poems.json`，但代码从不读它（默认诗词硬编码在
`libraries.cpp` 的 `PoetryLibrary::defaultPoems()`）—— 白占二进制体积，还留两份真相。
已从 qrc 移除，文件本身保留在 `resources/` 下作为示例数据。

> 未处理：该 JSON 带 UTF-8 BOM。`QJsonDocument::fromJson` 是否接受 BOM 未实测，
> 因此**没有**把它写进 README 当作"可直接导入的示例"。要当导入样例用的话需要先验一次。

**验证**：`poetry_tests` 20 项、`poetry_ui_tests` 7 项全过。

---

## 2026-09-24 — 第二轮界面调整：修掉重设计自己留下的重叠缺陷

用户截图暴露：「显示设置」页里 `字体族 / 正文基准字号 / 排版预设` 三行标签互相叠印，
`文字位置` 与 `铺满（Fill）` 也重叠。这不是配色问题，是**真实缺陷**，且是我上一轮重设计留下的。

### 根因（两层）

1. **纵向空间被抢**。上一轮把「操作卡」放进右侧栏，四个按钮 + 三行摘要吃掉约 280 px，
   下面的标签页只剩一半高度。`QFormLayout` 在高度不足时**不会换行也不会滚动，而是把行压到接近零高**，
   于是同页标签互相画叠。
2. **横向也偏窄**。右栏只有 ~470 px，图片库页内部仍是横向双列表，目录列表被挤到 118 px 宽。

### 改动

- **操作卡移出右栏**，改为预览图下方的**横向工具条**（媒体查看器式），摘要压成一行说明
  `当前预览：山居秋暝 · 王维 · bbb.jpg`，与提示语并排成一行 caption。右栏变成**纯面板栈**，
  标签页拿到整列高度。
- **每个设置页包进 `QScrollArea`**（`makeTabPage` 统一处理，`widgetResizable` + 关横向滚动条）。
  根治手段：内容永远保持自然高度，装不下就滚动，而不是被压扁。这一条也覆盖了最小窗口的情形。
- **图片库页改纵向堆叠**（目录在上 2 : 图片在下 3），适配窄而高的新面板；
  图片库与诗词库的工具栏都改为 **2×2 / 3×2 网格**，避免窄栏下按钮溢出。
- **整体收紧控件尺寸**：按钮 `min-height 34→28`、内边距 `5px 14px→2px 11px`、字号 `12→11pt`；
  下拉框/数字框 `32→28`；列表行 `30→24`；表头 `32→26`；标签页 `32→26`；卡片内边距同步缩小；
  诗词表格行高 `42→32`。主按钮 `48→44`。

> 主按钮停在 44 px 而不是更小，因为 `test_ui` 里固化了 `switchButton minimumHeight >= 44`
> 这条可访问性下限（对应 `REQUIREMENTS_SYNC` 的「默认可读」验收项）。要再小需要你确认改这条验收。

### 新增自动化测试

`UiTests::settingsControlsNeverOverlapEachOther`：遍历四个标签页，收集页内所有同类控件
（按钮 / 下拉框 / 数字框 / 复选框 / 标签 / 列表 / 表格），两两比较全局矩形，
**排除父子包含后断言不得相交**。在 1440×900 与 **最小窗口 980×680** 两个尺寸下各跑一遍。

这个测试不是写完就信的：它在 980×680 下**立刻抓到**两个下拉框纵向相交 3 px 的真实重叠，
也就是上面 `QScrollArea` 那条改动要解决的问题。先失败、修好后通过，才算有效守卫。

**验证**：`poetry_tests` 20 项、`poetry_ui_tests` **7 项**全过；无 error / 无 warning。

---

## 2026-09-24 — 随机切换只在两张图之间来回（严重）

**症状**：连点「随机换一张壁纸」，桌面只在这两张之间跳；57 张图片实际只用到 2 张，3 首诗词只用到 2 首。

**根因**：`MainWindow::randomSwitchWallpaper()` 把 picker **拷贝**进工作线程：

```cpp
const RandomPicker picker = m_picker;
QtConcurrent::run([..., picker, ...]{ return WallpaperService::run(..., picker, ...); });
```

`RandomPicker::pickIndex()` 推进的是拷贝内部的 `std::mt19937`，`m_picker` 自身的生成器状态
永远停在构造种子后的位置。于是每次点击都取出**同一个随机数**；再叠加"避免连续重复"那句
`if (candidate == lastIndex) candidate = (candidate + 1) % count;`，结果就是 X / Y 无限交替。

**改动**：

- 抽取移到 UI 线程。抽取是 O(n) 的轻量决策，工作线程只承担昂贵的渲染与设置。
  `WallpaperService` 拆出 `renderAndApply(poem, image, …)`，`run()` 保留并改为「抽取 + 委派」，
  行为不变，原有测试继续覆盖它。
- `run()` 的 picker 参数由**按值**改为 **`RandomPicker &`**。这样误用在编译期就不成立：
  不能再传临时对象，且调用方的状态必然被推进。
- `m_picker.rememberPoem/rememberImage` 从 `switchFinished` 移到抽取处。失败时也会前进，
  避免反复撞上同一张已知会失败的图片。

**新增测试**：

- `wallpaperServiceAdvancesTheCallersPicker`：同一个 picker 连调 `run()` 12 次，断言抽到的
  图片与诗词各超过 2 种。**按值传参时 12 次只会抽到 1 张**（`run()` 内部不调 remember，
  连交替都不会发生），因此该用例是真正的回归守卫。
- `randomPickerCoversThePoolAcrossRepeatedDraws`：picker 就地连续抽取必须覆盖足够大的样本池。

**验证**：全新配置构建，无 error / 无 warning；`poetry_tests` 20 项、`poetry_ui_tests` 6 项全过。

> 过程记录：增量构建时 **AUTOMOC 没有重新生成 `test_core.moc`**，导致新测试函数虽然编译进去
> 了却没注册进 QTest 槽表，跑出来仍是 19 项且毫无报错。删掉构建目录重来才暴露真实数量。
> 结论：给 Qt 项目加测试用例后，**别信增量构建的用例计数**；CLion 侧建议做一次
> Tools → CMake → Reset Cache and Reload Project 或完整重新构建。

---

## 2026-09-24 — 界面布局与视觉重设计

只动 UI 与布局，**功能、信号连接、控件 objectName、交互语义全部保持不变**。

### 设计判断

原布局的核心矛盾是：底部标签页和预览**抢同一份高度**，于是作为主角的预览被压到只剩半屏，
右侧栏又被塞进操作、摘要、状态三种内容，最终出现文字互相叠印。
另外把「素材管理 / 外观 / 自动化」三件性质完全不同的事情平铺成同级标签页，也没有优先级。

### 结构调整

- 骨架从「顶栏 + 预览/操作分栏 + 底部标签页」改为 **「顶栏 + 左预览（整列高度）+ 右侧栏 + 底部状态条」**
- 预览卡独占左列，拿到窗口约 78 % 的高度，成为真正的主角；左右仍是 `QSplitter`，用户可自行配平
- 右侧栏 = 常驻**操作卡** + 其下的**四页面板栈**（`resourceSettingsTabs` 仍是 4 个标签页）
- 操作卡内做**动作分级**：`随机换一张壁纸` 唯一主按钮（实心蓝 48px）；`应用当前预览` 降为描边强调
  （它只在存在未应用草稿时才有意义）；`重新预览` 与 `开启定时` 并列为次级
- **状态分三层**，消除重复表达：顶栏胶囊 = 桌面真实状态（唯一权威）；操作卡 = 预览内容摘要；
  **新增底部状态条** = 最近一次操作反馈。原先挤在右侧栏导致叠印的 `statusLabel` 移到这里
- 「显示设置」由一个 11 行的表单拆成 **排版 / 颜色 / 版面** 三张卡；三个取色按钮改为横排一组，
  从属关系一眼可见
- 「定时与高级」的定时项卡片化
- 诗词库工具栏由 6 个按钮一行改为 **3×2 网格**——侧栏变窄后一行放不下，会溢出
- 危险操作（移除目录 / 删除选中 / 清理缓存）统一红色描边；`恢复默认显示设置` 降为无框幽灵按钮

### 视觉系统

建立一套 design token 并以 objectName 选择器施加：中性色阶 `#0f172a / #334155 / #64748b / #e2e8f0 / #eef2f7`，
主色 `#2563eb`，圆角 8（控件）/ 12（卡片）/ 13（状态胶囊），间距节奏 8·10·12。
窗口底色改由 `QPalette::Window` 提供，不再依赖全局 `QWidget { color: … }` 这类会干扰控件自绘的写法。
字号阶梯用 objectName 精确指定，**基准字体仍是 12 pt**，因此不改变任何控件的 `font()` 取值。

### 新增自动化测试

`UiTests::layoutKeepsEveryRegionVisibleAndThePreviewDominant`：真实 `show()` 窗口后断言
常驻区域非退化且未越出窗口边界、预览占据主导（≥ 40 % 宽 / ≥ 45 % 高），并**逐个切换到四个标签页**
后再量该页关键控件的尺寸下限（图片双列表、诗词表、字体与字号控件、定时间隔）。

补这个测试的直接原因：上一轮「控件存在但完全不绘制」的回归，靠 objectName 断言是抓不到的。
过程中它也纠正了我自己写错的两处断言——底部状态条 17 px 是 10 pt 单行的正确高度（阈值写错），
以及 `QStackedWidget` 只布局当前页，非当前页的子控件停在 100×30 的未布局默认尺寸（须先切过去再量）。

### 未经机器验证的部分

配色、圆角、留白是否"好看"无法自动断言，需要人眼确认。几何关系已由上述测试覆盖。

---

## 2026-09-24 — 缺陷修复批次

范围取自代码审查与真机运行后肉眼确认的问题清单。只收录**无需先裁定文档冲突**的项；
涉及排版规范数值（字号 / 行距 / 安全区 / 缩放公式）的偏差全部未动，原因见文末「本批未做」。

自测：`poetry_tests` 18 项、`poetry_ui_tests` 5 项全部通过；三个目标编译无 warning。

---

### 1. 预览缩放挂错事件，导致残影、底部裁切、文字重叠

**症状**：预览图底部最后一行诗词被横向切掉；「修改显示设置后会实时更新预览…」提示文字画在了图片内部；
右侧「当前预览内容」与状态文字互相叠在一段，两者都读不出来。最大化/还原、拖动任一分隔条都会触发。

**根因**：缩放发生在 `MainWindow::resizeEvent` 里，而此刻中央控件的 layout 尚未把新尺寸传导给子控件，
`m_previewLabel->size()` 拿到的是旧的、更大的值；pixmap 按旧尺寸缩放后 label 变小，多出的区域留着上一帧像素。
更糟的是拖动 splitter 根本不触发窗口 resize，预览完全不再重算。

**改动**（`src/ui/main_window.{h,cpp}`）：

- 删除 `MainWindow::resizeEvent`，改为给 `m_previewLabel` 装事件过滤器，响应它自己的 `QEvent::Resize`
- 新增 `m_previewPixmap` 缓存全尺寸图，`updatePreview()` 只负责读盘一次，缩放交给新的 `rescalePreview()`
  （原先每次 resize 都重新解码那张约 2.7 MB 的 PNG）
- `m_rescalingPreview` 重入保护

**代价**：常驻内存增加约 32 MB（全尺寸 `QPixmap`）。峰值 159 MB，仍在 `PRODUCT_SPEC` 的 ≤200 MB 门槛内。

**附带修正**：`<QResizeEvent>` 原先顺带提供了 `QCloseEvent` 的完整定义，移除后编译失败，
已显式补 `#include <QCloseEvent>` 并把 include 按字母序归位。

---

### 2. 壁纸输出尺寸扣掉了任务栏

**症状**：4K 屏（3840×2160，125% 缩放）上产出的壁纸是 **3840×2100**，少 60 物理像素。
Windows 默认按「填充」显示，会放大 1.0286 倍后左右各裁约 54 px —— 而诗词面板正贴在右边缘，只剩约 21 px 余量。

**根因**：`targetWallpaperSize()` 用了 `QScreen::availableGeometry()`（已扣除任务栏）。

**改动**（`src/ui/main_window.cpp`）：改用 `QScreen::geometry()`。

**同时**（`src/platform/windows_wallpaper_setter.cpp`）：设置壁纸前写
`HKCU\Control Panel\Desktop` 的 `WallpaperStyle=10` / `TileWallpaper=0`。
合成图已经严格等于屏幕尺寸，「填充」是唯一不会裁切也不会留黑边的样式；
此前不写注册表，显示效果取决于用户既有的桌面设置，行为不确定。
`CMakeLists.txt` 三个目标相应增加 `advapi32` 链接。

---

### 3. 图片来源列表中文变成字面问号，且不区分目录与单文件

**症状**：列表项显示为 `?? ? S:/file/.../before`。

**根因**：某次非 UTF-8 编辑把中文写成了 `?`（`QStringLiteral("?? ? %1")`）。
另外「单文件」和「目录」两个分支的前缀文字完全相同，即使修好编码，`isFile` 状态在界面上也没有表达出来。

**改动**（`src/ui/main_window.cpp`）：前缀改为「图片  」/「目录  」，两种来源可区分。

**本项附带的一次回退**：顺手给目录列表加了 `setTextElideMode(Qt::ElideMiddle)` 和
`setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff)`，想让长路径中间省略而不是出横向滚动条
（那是清单外的 P2 小瑕疵）。实测导致**目录列表整个变空**——配置里 `imageSources` 完好，界面上
连复选框都不再绘制。两行已回退到改动前状态。教训：不在同一批里夹带超出范围的"顺手改"，
尤其当它触及核心控件时；宁可留着那条滚动条。

> 编码根因属于工具链配置，已在 `README.md`「CLion 配置 4」要求把 Project Encoding 设为 UTF-8。

---

### 4. 声称支持的格式缺解码插件时被静默吞掉

**症状**：`.webp` 在扫描白名单里，但 Qt 无 `qwebp.dll`，这类文件既不计数也不报错，用户完全看不到原因。

**改动**（`src/core/libraries.cpp`）：`scan()` 的两个分支原本近乎重复的校验逻辑抽成共享 lambda `acceptImage`，
并区分三种情况——扩展名不在支持清单（正常忽略，不计数）、在清单但运行时缺插件（计入 `skipped` 并**每种格式只报一次**
「Qt 缺少 WEBP 解码插件，该类型文件已全部跳过」）、文件本身读不出（计入 `skipped` 并逐个报路径）。

**新增测试**：`imageScanReportsUnavailableFormatsInsteadOfSilentlySkipping`。
构造 `ok.png` + `notes.txt` + `shot.webp` 三种文件，断言只有 png 入选、非图片扩展名不污染 `skipped` 计数、
且在缺 WEBP 插件的环境下必须报出 WEBP。该用例在修改前的代码上会失败。

---

### 5. 扫描时全量解码每张候选图

**症状**：仅为验证可读性就 `reader.read()` 整图解码，照片目录下扫描明显卡顿、内存峰值高。

**改动**（`src/core/libraries.cpp`）：改为 `canRead()` + `size()` 的头部校验，不再解码像素。

**行为变化（已知取舍）**：结构完整但内容损坏的图片现在会在真正合成时于 `renderPreview()` 报错，
而不是扫描阶段就被剔除。失败路径本身是安全的——不会改桌面、不会删缓存、保留上一张有效预览。
换来的是扫描开销与图片体积基本无关，符合 `PRODUCT_SPEC` 的「200 图保持响应」要求。

---

### 6. 「更多」按钮出现两个下拉箭头

**根因**：文字里手写了 `▾`，同时 `QToolButton::InstantPopup` 会自己绘制 menu-indicator。

**改动**（`src/ui/main_window.cpp`）：去掉文字里的 `▾`，只保留 Qt 绘制的那个。

---

### 7. 图片库页信息优先级倒置

**症状**：「当前选中图片」占 3 行（文件名、尺寸、一整句语义说明），把主操作区「可用图片」列表压到只剩一行可视高度。

**改动**（`src/ui/main_window.cpp`）：

- 选中信息压成一行 `当前选中：bbb.jpg · 1500 × 1013 · 116 KB`，完整路径进 tooltip
- 「预览不改桌面 / 原图直接改桌面」的语义说明移到两个动作按钮的 tooltip 上
- 移除该 label 的 `setWordWrap(true)`（不再需要多行）
- `m_imageFiles` 设 `setMinimumHeight(140)`，保证至少 4 行可视

---

### 8. 结果消息里带完整绝对路径

**症状**：`已应用到桌面：静夜思 / S:/file/File/desktopBackground/before/bbb.jpg` 这类长文本会撑爆右侧状态栏，
是第 7 项重叠的诱因之一。

**改动**（`src/core/wallpaper_service.cpp`）：`run()` 的成功消息改用 `QFileInfo(...).fileName()`。
完整路径本来就显示在「当前预览内容」和图片 tooltip 里。

---

### 9. `lastMessage` 是从不写入的死字段

**症状**：`RuntimeState::lastMessage` 被序列化、被反序列化，但代码里没有任何地方给它赋值，
实测持久化结果永远是 `""`。

**改动**（`src/core/models.h`、`src/core/config_store.cpp`）：删除该字段及其两个序列化分支。
旧 `config.json` 里残留的 `lastMessage` 键会被 `configFromJson` 忽略，无需迁移。

> 若日后要做「重启后显示上次切换结果」的功能，应当作为新需求重新引入并配套展示逻辑，
> 而不是留一个永不生效的占位字段。

---

### 10. 落款缺少规范要求的间隔号

**症状**：合成图上「李白」与「唐」只是并排的两段文字，中间只有一道空隙。`TYPOGRAPHY_SPEC` 要求 `作者 · 朝代`。

**改动**（`src/core/renderer.cpp`）：`TypographyLayout` 增加 `separator`，并排模式下用朝代字号绘制 `·`，
左右各留一个 `metadataInlineSpacing`；并排可行性的宽度判定同步把分隔符算进去
（放不下时仍回落到作者、朝代各占一行的堆叠模式，堆叠模式不加间隔号）。

顺带修正该分支里一处 `settings.shadowEnabled` 与 `effective.shadowEnabled` 混用的不一致写法。

**未加单元测试**：作者与朝代目前共用 `metadataColor`，间隔号无法用颜色从像素中分离出来，
任何断言都会退化成"朝代有没有画"而不是"间隔号有没有画"。待补 `dynastyColor` 字段后再加用例。

---

## 本批未做（及原因）

| 项 | 为什么搁置 |
|---|---|
| 正文行距 1.35 / 标题落款行距 / 安全区 54%·82% / 边距 6% / 缩放公式 / 溢出下限 10pt | 属规范数值冲突区。`TYPOGRAPHY_SPEC` 与 `REQUIREMENTS_SYNC` 对字号（28/32/36）、默认锚点（左下/右下）、行距（1.35/1.60）互相矛盾，代码取的是较新的 `REQUIREMENTS_SYNC` 基线。改数值前需要先定唯一真相源 |
| 朝代独立颜色（规范要 `#B8FFFFFF`，与作者 `#EBFFFFFF` 分层） | 需要给 `RenderSettings` 加字段、配置读写兼容、UI 增加取色控件，属独立一轮设计变更；且它也是第 10 项补测试的前置 |
| 暗底图上自动减弱/关闭文字面板 | 需要按图片亮度做决策，是新功能 |
| 预览 1:1 或局部放大查看 | 新功能 |
| 应用与托盘图标（现为 Qt 默认图标 / `SP_ComputerIcon`） | 需要设计素材 |
| `resources.qrc` 打包了从没人读的 `default_poems.json` | 二选一（接上运行时读取 / 删掉死资源）取决于产品意图，需先确认 |
| 目录列表长路径出横向滚动条（期望中间省略） | 本批试过，导致列表整个不渲染，已回退（见第 3 项）。需要单独排查 `QListView` 的 elide 与该控件的交互后再做 |
| 中英混排按字符断词、排版预设不回落「自定义」、`canvasBackgroundColor` 无 UI 控件 | 非本轮"明显缺陷"范围 |
| 构建产物入库（1164/1199 个跟踪文件）、`qa-build/` 未列入 `.gitignore` | 需要 `git rm --cached` 并单独提交，待批准 |
