# Atom 字形渲染发展路线（架构线 + 性能线合并版）

> 状态：W0–W3 已落地（见文末「实施状态」），W4–W6 待排期。本文把 `TextLayout-Design-CN.md`（架构）、
> `TextLayout-Implementation-Plan-CN.md`（分阶段计划）与性能审计中的文本相关 P0
> （RENDER-012 字形栅格化同步、ARCH-107 异步加载、RENDER-010 HarfBuzz shaping）
> 合并为一条可执行的路线。
>
> 说明：stb 单张纹理解码已**不属字形渲染链**——它只服务封面/贴图渲染，
> 且已通过 `TextureCache::LoadAsync` 完成"worker 解码 → 渲染线程建纹理"的
> 拆分，因此本文不再把 stb 列为文本线的待办项。
>
> 核心判断：两条线不是两条路，是同一条路——**帧边界上传队列 + 延迟释放协议**
> 是文本线 W2 起所有 GPU 触点的共同地基（字形页上传、atlas 淘汰、SDF 烘焙）。

## 为什么合并

| 性能 P0 | 与文本线的关系 |
|---|---|
| RENDER-012（EndFrame 同步栅格化，CJK 首帧卡顿，单 FT_Face 加锁） | 就是文本线自己的问题，W2 直接消化 |
| ARCH-107（future/handle + 进度 + 世代 + 帧边界回传） | 字体加载同样要走它（README 已禁止渲染线程首次遇到 CJK 同步加载字体） |
| RENDER-010（HarfBuzz 纯 CPU，首版就应异步+缓存） | W2 直接消化 |

设计文档"引入前决策门槛"最后一条（先完成 `IRender2DContext` 延迟释放协议再启用
atlas LRU）已经隐含了这个依赖，这里把它显式化为 W0。

## W0 · 地基（性能线，与 W1–W3 并行）

1. **帧边界 GPU 上传队列**（带版本号 + 字节预算）
   - `CreateTexture`/上传只允许发生在帧边界（Renderer2D 的 `pending_uploads_`
     已在 `EndFrame()` 统一上传，`CreateTexture` 本身只暂存像素）；
   - 恢复 glyph 页脏矩形/分片上传（README 记载该项"待上传队列完成后恢复"）；
   - 未来 SDF 烘焙产物上传复用同一队列。
2. **GPU 资源延迟释放协议**：淘汰与提交中 draw packet 协调；atlas LRU 的前置。
3. **ARCH-107**：future/handle + 进度 + 世代 + 帧边界回传；MusicCard 手写
   LoaderLoop 退役；字体字节加载接入同一机制。

## W1 · 契约冻结（立即做，纯头文件，与 UI P1 并行）

对原计划 P0 的修订：

- 四个头文件照旧：`TextLayout.hpp` / `FontProvider.hpp` / `GlyphAtlas.hpp` /
  `TextRenderer.hpp`（后端无关，值类型 + 纯虚接口）。
- **修订 1（方向性）**：把"字形栅格表示"提升为一等维度，不要埋在 `render_mode`
  里。atlas key 显式含：

  ```cpp
  enum class RasterFormat : uint8_t {
      AlphaBitmap, LCD, Mono, SDF, MSDF, ColorBitmap
  };
  ```

  TextRenderer 的 draw packet 按 format 选材质（位图 → alpha blend；
  SDF/MSDF → screen-space px-range 距离场 shader）。这样未来加 SDF 是
  "新增一个 provider + 一种 atlas + 一个 shader"，而不是把 atlas key 迁移第二次。
- **修订 2（原则写死）**：fallback 字体自带打包（如 Noto Sans + Noto CJK），
  不依赖 fontconfig/CoreText/DWrite，保证跨平台渲染可复现、golden test 可成立。
- 同时冻结 layout cache 的 key 与失效条件（文本、宽度、字体、DPI、feature、locale）。

## W2 · CPU 装箱与异步化（消 RENDER-012 + RENDER-010）

1. **FreeTypeFontProvider**：`Font` 内部逻辑迁入，`Font` 变薄适配（原计划 P1 不变）。
2. **栅格化 job 化**（原计划没有，本波新增）：
   - 走 JobSystem fork-join（对齐 `AudioMetadataReader::ReadBatch` 模式）；
   - FreeType 线程规则：同一 `FT_Face` 不允许并发使用 → **per-worker
     FT_Library + FT_Face clone**（或任务级 clone），不做全局加锁；
   - 新字形**延迟一帧**：本帧先以占位渲染（或跳过），栅格结果经上传队列在
     帧边界入库，下一帧替换——消除 CJK 首帧卡顿，不需要在渲染线程同步等栅格。
3. **HarfBuzzTextShaper**（原计划 P2）：
   - `Shape(utf8, direction, script, lang, features) → glyph_id/cluster/advance/offset`；
   - **首版就异步**：shaping 纯 CPU，可 job 化；per-thread `hb_buffer`；
   - run 级缓存（同字体+字号+script+lang+文本片段 → glyph run），immutable
     天然可跨帧复用。

## W3 · immutable TextLayout（UI 控件的消费基座）

- `TextLayout::Build(utf8, width, align, ...)` 产出不可变 glyph/行盒；
- `Renderer2D::DrawTextLayout(const TextLayout&, x, y, color)`；
- 旧 `DrawText` 内部临时 Build+缓存，业务 API 不破坏；
- `MeasureText` 走 `TextLayout::Bounds`，与 `DrawText` 共用单一排版真源；
- `text_layout_cache_`（128 条字符串键 memo）退役；
- **时序约束**：UI P1 的文本框（光标、hit-test）、滚动列表、长标题测量
  全部建在 TextLayout 上，本波必须与 UI 控件成型同步或更早。

## W4 · atlas key 升级 + fallback（唯一破坏性内部改动，一次到位）

- key：`(FontInstanceId, glyph_id, raster_px, raster_format, render_mode)`；
- 打包字体 fallback 链（主字体缺字 → 后备字体）；
- atlas LRU + 显存字节预算（依赖 W0 延迟释放协议）；
- 彩色 emoji/COLR 的 `ColorBitmap` 槽位已预留（`EnsureGlyph` 现有 page 即 RGBA，
  已处理 `FT_PIXEL_MODE_BGRA`）。

## W5 · golden tests + 切换

- 基线覆盖：Latin / CJK / Arabic / 组合字符 / 换行 / 裁剪 / DPI / atlas 淘汰 /
  字体销毁与在用 layout 生命周期；
- 删除 `Renderer2D` 内联 UTF-8 解码 / kerning / wrap；Renderer2D 只消费 glyph quad。

## W6 · 增量（按需，不做进排期硬承诺）

1. **libunibreak（UAX #14/UAX #29）**：换行是所有语种的硬需求，不必等 ICU；
   与 bidi 解耦，别把轻量换行和 20MB+ 的 ICU 绑在同一阶段。
2. **SDF/MSDF 路径**：新增 SDF provider + atlas format + 距离场 shader，
   消费端不变（W1 冻结的 format 维度保证）；用于 nametag / 伤害数字 /
   3D 标牌 / 标题字等世界与特效文本。
3. **离线烘焙**：标题字/特效字由 cook 阶段烘焙 SDF atlas + metrics，
   对接 Packager，与运行时产物共用同一消费链路。
4. **ICU bidi**：仅当阿拉伯语/希伯来语成为硬需求时引入。

## 明确不做

- 不引入 Skia / Cosmic-Text（第一阶段）；ICU 延后见 W6；
- 不做全局 FT 锁（用 per-worker face）；不做全局 shader 改造（format 选材质即可）；
- W4 之外不破坏任何公开 API；W4 整阶段一次切、保留旧 key 兜底。

## 与现有文档的关系

- `TextLayout-Implementation-Plan-CN.md` 的 P1/P2/P3/P5 内容分别并入本路线
  W2/W2/W3/W5；原 P0 升级为 W1 并加 `raster_format` 修订；原 P4 升级为 W4
  并扩展 format 维度；原"P3 备用"的定级改为 **W1 立即、W2–W3 随 UI 走**，
  W4–W5 仍可保持备用定级。

## 实施状态（截至 2026-10-23）

| 波次 | 内容 | 状态 |
|---|---|---|
| W0 | 帧边界 GPU 上传队列 + 延迟释放协议 + ARCH-107（Future/Promise + AcquireAsync） | ✅ 完成 |
| W1 | 契约冻结（TextTypes / FontProvider / TextShaper / TextLayout / GlyphAtlas / TextRenderer，raster_format 一等维度） | ✅ 完成 |
| W2 | FreeTypeFontProvider（W2a）+ 栅格化 job 化（W2b）+ HarfBuzzTextShaper（W2c） | ✅ 完成 |
| W3 | immutable `TextLayoutBuilder`（纯 CPU 排版真源） | ✅ 完成 |
| W3 | Renderer2D 集成（`DrawTextLayout` / 旧 `DrawText` 切换 / `MeasureText` 单一真源 / 退役 `text_layout_cache_`） | ⏳ 未做 |
| W4 | atlas key 升级 `(FontInstanceId, glyph_id, ...)` + 旧 `Font` 迁移 + fallback 链 | ⏳ 未做 |
| W5 | golden tests + 切换到 glyph quad 消费 | ⏳ 未做 |
| W6 | libunibreak / SDF / 离线烘焙 / ICU bidi（增量） | ⏳ 未做 |

### 剩余工作（按依赖顺序）

1. **W3 收尾**：让 Renderer2D 消费 `TextLayoutBuilder`。前置是 W4 的 atlas key
   升级——当前旧 `Font` 仍以 `(Font*, codepoint)` 为 key，新管线以
   `(FontInstanceId, glyph_id)` 为 key，二者不能混用，所以 Renderer2D 集成必须
   等 W4 一起做（否则要维护两套 key 空间）。
2. **W4**：atlas key 升级 + 旧 `Font` 迁薄 + fallback 链（`FallbackFor` 当前返回空）。
3. **W5**：golden tests（Latin/CJK/Arabic/组合字符/换行/裁剪/DPI/atlas 淘汰/字体销毁）
   + 删除 Renderer2D 内联 UTF-8 解码 / kerning / wrap。

### 已知局限（第一版）

- **tab 降级为单空格**：`TextLayoutBuilder` 把 tab 1:1 替换为空格做 shape 与测宽，
  不做四空格列对齐（对齐现有 Renderer2D 语义，后续若需列对齐再补 tab stop 表）。
- **新老管线隔离并存**：`atom::text` 新管线与旧 `atom::render::Font` 路径互不感知，
  在 W4 切换前二者并行存在，业务层仍走旧路径，无行为变化。
- **fallback 未实现**：`FreeTypeFontProvider::FallbackFor` 返回空，多字体 fallback
  留到 W4。

### 注意事项（潜在风险，落地后复查）

- **`TextureCache::LoadAsync(filesystem)` 引用生命周期**：worker lambda 按引用捕获
  文件系统，要求引用 outlive 请求。MusicCard 用成员 `background_filesystem_` 保证，
  但 Shutdown 时须先等 `background_load_` 落地再 reset 文件系统，顺序敏感。
- **`ResourceManager::AcquireAsync` promise 生命周期**：回调以 shared_ptr 持有
  promise，caller 若丢弃 future 且从不 `PumpAsync`，回调永不触发、promise 无 GC
  回收——这是 pull 模型的已知权衡，需由调用方保证每帧 Pump 或主动 Cancel。
- **`TextureCache::EvictAll` 不清 `in_flight_`**：与 `ResourceManager::Evict`
  （会取消在途）语义不一致，在途 decode 完成后仍会 materialize 回填 cache。
  当前无实际危害，待 W4 atlas LRU 引入淘汰时统一取消语义。
