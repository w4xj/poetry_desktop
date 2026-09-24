# 变更说明

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
