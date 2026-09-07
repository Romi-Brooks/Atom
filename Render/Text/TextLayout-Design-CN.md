# Atom TextLayout 设计记录

> 状态：提案。本文记录当前 stb 路径的边界、已完成修复，以及迁移至 FreeType + HarfBuzz 前必须稳定的契约；不是本版本立即引入新三方库的决定。

## 当前实现与已完成修复

当前 `Renderer2D` 仍直接将 UTF-8 解码为 codepoint、按单字体 `stb_truetype` 计算 advance/kerning、栅格化并管理动态图集。该路径适合单字体、简单左到右文本与轻量 HUD。

本版本已完成：

- UTF-8 非法序列与 surrogate 校验；
- ASCII 单词整体换行，CJK 等非 ASCII 字符仍可按字符断行；
- CRLF 的 `\r` 忽略、tab 对齐至下一个四空格宽的列停靠点；
- 缺字尝试栅格化当前字体的 `.notdef` glyph；
- atlas 尺号量化为 0.5px，避免连续浮点字号无限创建 atlas；
- 文本使用线性采样；新 atlas 页首次清零，后续字形使用脏矩形上传；
- 超出 atlas 页的字形产生可定位日志，未使用的 glyph advance 字段已移除。

这些修复不改变 stb 的能力上限：`.notdef` 不是字体 fallback，手写 word wrap 不是 Unicode line breaking，codepoint 也不是最终 glyph。

## 未解决的问题

1. 真实 fallback：一个文本 run 可能由多个字体覆盖，选择字体会影响 advance、baseline、glyph id 与 atlas key。
2. shaping：连字、组合音标、阿拉伯文连接、Indic 重排、RTL、OpenType feature/variation 不能由逐 codepoint 渲染获得正确结果。
3. 排版：UAX #14 换行、词典断词、Unicode bidi、script/language run 划分、对齐、ellipsis 与 hit test 不应留在 `Renderer2D`。
4. 资源策略：atlas 需要按显存/CPU 预算 LRU；淘汰与 GPU 延迟释放、仍被提交中的 draw packet 协调。
5. DPI：字体实例、hinting 与 atlas 必须以实际物理像素和 variation 参数为 key。

## 目标流水线

```text
UTF-8
  → TextLayout（Unicode 分段、fallback、换行、行盒）
  → TextShaper（HarfBuzz：glyph id、cluster、advance、offset）
  → GlyphAtlas（FontInstance + glyph id + raster 参数）
  → TextRenderer（普通 glyph quad / draw packet）
```

`Renderer2D` 最终只应消费已经定位好的 glyph quad；它不应再解码 UTF-8、读取字体、决定 fallback 或计算 kerning。

建议的最小公开数据：

```cpp
struct PositionedGlyph {
    FontInstanceId font_instance;
    uint32_t glyph_id;
    uint32_t cluster_byte_offset;
    float x, y;
    float x_advance, y_advance;
};

struct TextLine {
    std::span<const PositionedGlyph> glyphs;
    float baseline, width, ascent, descent;
};

class TextLayout { /* immutable lines, bounds, source-to-cluster mapping */ };
```

atlas 的 key 必须从当前的 `(Font*, size_px, codepoint)` 演进为至少 `(FontInstanceId, glyph_id, raster_px, render_mode)`；否则 HarfBuzz 的输出不能正确复用。

## FreeType 与 HarfBuzz 的职责

- **FreeType**：加载 font face/variation、选择像素尺寸、hinting、加载 glyph，并栅格化 alpha 或彩色 bitmap。
- **HarfBuzz**：按 direction/script/language/features 将 Unicode run shape 为有 glyph id、cluster、advance、offset 的输出。它不替代 rasterizer 或 atlas。
- **Unicode 分段层**：负责 bidi、script run、line break 与 fallback 选择；第一阶段可使用小型实现，完整能力可评估 ICU。

FreeType without HarfBuzz 仍无法正确处理复杂脚本；HarfBuzz without FreeType 可 shaping，但常规 UI 字体栅格、hinting 与色彩 glyph 路径仍不完整。因此生产多语言路径应采用两者组合。

## 迁移策略

1. 先冻结 `TextLayout`、`FontProvider`、`GlyphAtlas`、`TextRenderer` 的后端无关接口，并把当前 stb 路径包装为 `StbFontProvider`。
2. 将现有 `Renderer2D::ExpandTextOp` 拆为适配层；保留旧 `DrawText`，内部临时创建/缓存 `TextLayout`，避免一次性破坏业务 API。
3. 增加 `FreeTypeFontProvider` 与 `HarfBuzzTextShaper`，先覆盖 Latin + CJK + combining marks，再覆盖 Arabic/Indic/RTL 与彩色 emoji。
4. 建立 golden tests：相同字体、字号与文本在 layout glyph 序列、行宽、baseline、截图上的预期结果；覆盖 fallback 与 atlas 淘汰。
5. 当生产路径稳定后，把 stb 保留为可关闭 shaping 的轻量构建选项，而非删除。

## 成本与收益判断

引入两库会增加依赖、构建配置和二进制体积；不会消除 atlas 或 GPU 上传成本。收益是把当前和未来的复杂文本逻辑交给成熟库，并使不可变 layout 可缓存：静态 UI 不再每帧重做 UTF-8 解码、kerning 和 wrap。对于仅有固定西文字体的极简程序，stb 仍可以是更小的选择；对于 Atom 需要支持的多语言 UI，FreeType + HarfBuzz 是推荐生产路径。

## 引入前的决策门槛

- 明确 FontInstance 的生命周期、fallback 策略和字体资产/VFS 接口；
- 明确 layout cache 的 key 与失效条件（文本、宽度、字体、DPI、feature、locale）；
- 确定第三方引入方式、跨平台构建、许可证归档与离线构建；
- 先完成当前 `IRender2DContext` 的 GPU 资源延迟释放协议，再启用 atlas LRU。
