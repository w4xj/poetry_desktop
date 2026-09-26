# Requirements Sync and Acceptance Record

- **Status**: agreed for implementation and product acceptance
- **Date**: 2026-08-07
- **Roles**: Product, Programmer, Test
- **Scope**: desktop poetry wallpaper tool redesign

## 0. 数值优先级裁定（2026-09-26）

四份文档在字号、默认锚点、行距、面板色上互相矛盾，长期导致"改哪个都不对"。
**本文件是渲染数值的唯一权威**，理由：它是唯一标记为 *agreed for implementation and product
acceptance* 的文档，且日期最新（2026-08-07，晚于 TYPOGRAPHY_SPEC 的 08-05）。

| 数值 | 权威值（本文件） | 被覆盖的说法 |
|---|---|---|
| 正文基准字号 | 36 pt @1920×1080，随分辨率缩放 | `PRODUCT_SPEC` / `UI_REDESIGN` 的 28；`TYPOGRAPHY_SPEC` 的 32 px |
| 默认文字锚点 | `bottomRight` | `TYPOGRAPHY_SPEC` 的「雅致左下」`bottomLeft` |
| 正文行距 | 1.35 | `TYPOGRAPHY_SPEC` 的 1.60 及其 TY-012 的 1.4–1.8 区间 |
| 面板默认色 | `#99000000`（60% 黑） | `TYPOGRAPHY_SPEC` 的 `rgba(0,0,0,0.42)` |

`TYPOGRAPHY_SPEC.md` 中**非数值的结构性规则仍然有效**，且已实现：标题 / 落款 / 正文三块独立排版
（不得 join 成一个字符串一次 `drawText`）、落款组合为 `作者 · 朝代` 带间隔号、朝代比作者低一档
透明度、层级靠字阶与透明度而非颜色种类、空字段自动收缩、溢出时逐级缩小而非静默裁剪、
正文为空禁止生成、预览与壁纸走同一渲染路径、排版改动 220 ms 防抖。

代码侧对应关系：`src/core/models.h` 的 `RenderSettings` 默认值即上表；改动数值前先改这里。

## 1. Product conclusion

The original complaint is valid: wallpaper typography must be judged at the final wallpaper resolution, not only inside the settings form. A normal user should not need to zoom the preview to read the poem. The product baseline is:

- Body text default: 36 pt logical size at 1920x1080. The renderer scales with resolution and keeps a safe area.
- Title: 1.50x body size, semibold, high-contrast color.
- Author and dynasty: smaller metadata sizes (0.72x and 0.68x), secondary color.
- Body: normal weight, 1.35 line spacing, separate body color.
- Text placement: top-left, top-right, center, bottom-left, bottom-right.
- Style: preset, font family, body size, title/metadata/body colors, panel, panel color, shadow and image fit are adjustable.
- UI text baseline: 12 pt, 28 px controls, 44 px primary action. Minimum window: 900x640 (logical).

## 2. Agreed information architecture

1. **Header**: application name, desktop status, image/poem counts, schedule summary, low-frequency menu.
2. **First screen**: large wallpaper preview on the left; quick actions on the right.
   - Random switch
   - Apply current preview
   - Refresh preview
   - Start/stop schedule
3. **Resource tabs**:
   - Image library: add file, add directory, remove directory, rescan, select one image, preview selected image, set original image as desktop wallpaper.
   - Poetry library: add, edit, import JSON, enable/disable, delete, preview selected poem.
   - Display settings: typography, location, fit mode, panel and shadow.
   - Schedule and advanced: enable/stop, interval 1-1440 minutes, next-run time, cache and log actions.

Preview and desktop are deliberately different states. Changing settings or selecting a resource refreshes preview only; desktop changes only after an explicit apply or a random switch.

## 3. Programmer questions and decisions

| Question | Decision |
|---|---|
| Does changing a font update the preview? | Yes. Settings are persisted and a debounced preview render is scheduled. |
| Does preview overwrite desktop wallpaper? | No. Only the explicit apply action or random switch calls the wallpaper setter. |
| What does selecting one image mean? | The selected image is retained as the next preview input. It must not be replaced by a random image. |
| What does the original-image action mean? | It is explicitly named as setting the original image, with no poem overlay. Poem wallpaper remains available through preview/apply. |
| What happens if settings change during rendering? | Generation tokens prevent stale results from replacing the latest requested preview. |
| What happens when the interval changes while scheduling is active? | The current timer is stopped and restarted with the new interval; next-run text is updated immediately. |
| What happens after one scheduled failure? | The current desktop is preserved, the error is logged, and the next cycle remains scheduled. |
| What is persisted? | Image sources, poem entries, render settings, schedule settings and last preview/runtime state. |

## 4. Test questions and acceptance answers

- **Readability**: all key controls are at least 11-12 pt; primary action is at least 48 px high; wallpaper defaults are resolution-scaled.
- **Missing resources**: switching is disabled or reports a concrete next action when images or enabled poems are absent.
- **Failure protection**: render/set failures never delete or replace the last successful wallpaper.
- **Schedule**: enable, stop, change interval while active, restart application, and tray pause/resume are testable.
- **Persistence**: restart retains resource lists, render settings, poems and schedule state.
- **Responsive layout**: main window supports 900x640 logical minimum without clipping key actions; high DPI uses Qt logical sizing. 1366x768 at 125% scaling is explicitly out of support (see README resolution table).
- **Accessibility**: primary controls and management controls have stable object names for automated UI tests.

## 5. Product acceptance checklist

### Must pass

- [x] Add/remove image sources and select a single image.
- [x] Add/edit/delete/enable/import poems.
- [x] Distinguish title, author/dynasty and body in rendered output.
- [x] Default typography is readable and resolution-aware.
- [x] Adjust font, size, colors, position, fit, panel and shadow.
- [x] Preview updates without changing desktop.
- [x] Apply current preview without re-randomizing content.
- [x] Random switch generates and sets a new wallpaper.
- [x] Start, stop and modify schedule interval while running.
- [x] Tray actions continue to work when the main window is hidden.
- [x] Cache cleanup protects current and preview files.
- [x] Core and UI automated tests pass.

### Reject and iterate if any occurs

1. The poem is visibly too small at 1920x1080 or 4K.
2. Title, metadata and body look like one undifferentiated text block.
3. A preview operation changes the desktop unexpectedly.
4. Applying the current preview generates a different poem or image.
5. Changing the interval leaves the old timer active.
6. An error overwrites the last successful wallpaper or hides the reason.
7. Key actions are clipped or require searching through a crowded settings page.

## 6. Delivery status

The implementation now includes the agreed first-screen layout, readable UI baseline, resource management, layered typography renderer, explicit preview/apply semantics, schedule controls, cache protection and automated core/UI tests. Final acceptance is based on the checklist above plus a Windows 10/11 real-desktop verification of wallpaper API behavior.
