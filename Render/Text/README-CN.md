# Atom Text 模块最佳实践

## 模块边界

`Render/Text` 是 CPU 侧的字体、排版和字形缓存服务；它不依赖 SDL_GPU、Vulkan、D3D、Metal 或具体窗口。渲染后端只接收已经生成的 glyph quad、纹理和材质。

当前 `Font` 是 stb_truetype 的单字体 provider，保留它是为了让 Atom 在没有系统字体库时仍能跨平台构建。新代码直接使用：

```cpp
#include <Render/Text/Font.hpp>
```

## 当前 provider 的适用范围

stb_truetype 适合受信任的 TTF/TTC、单字体、简单左到右文字、轻量工具和基础 HUD。它不负责字体 fallback、复杂文字 shaping、双向文字、组合字符、字体变体轴、hinting 策略或持久化 atlas。不要在 `Font` 中继续增加这些业务开关。

## 推荐的文字流水线

```text
FontProvider → TextShaper → immutable TextLayout → GlyphAtlas → TextRenderer
```

- `FontProvider` 管理字体族、字体实例和 fallback 链；
- `TextShaper` 将 UTF-8 转为 glyph run，处理 kerning、组合字符和脚本规则；
- `TextLayout` 保存不可变的 glyph、位置、换行、对齐和裁剪结果；
- `GlyphAtlas` 管理 GPU 图集、采样、淘汰和 DPI 版本；
- `TextRenderer` 只消费 `TextLayout` 并提交普通 draw packet。

Renderer2D 不应解析 UTF-8、读取字体文件或直接调用 stb。

## FreeType + HarfBuzz 迁移

生产级多语言路径推荐 FreeType（解析、rasterization、hinting）+ HarfBuzz（复杂文字 shaping、kerning、组合字符和脚本规则），ICU 可作为更完整的 Unicode 分段和双向文字扩展。

当前不立即替换的原因是仓库还没有 `TextShaper/TextLayout/fallback` 契约；直接把 stb 实现替换为 FreeType 只会增加包体和构建负担，无法解决 Renderer2D 过度承担文字职责的问题。完成这些接口后，新增 `FreeTypeFontProvider` 和 `HarfBuzzTextShaper` 即可替换，不需要改 SDL_GPU 后端。

切换条件：TextLayout API 稳定；CJK、拉丁、阿拉伯文和组合字符 golden test 建立；依赖的跨平台构建和许可证归档完成；atlas 上传和 DPI 缩放不再依赖 Renderer2D 内部状态。

## 资源与线程规则

- 字体字节和 provider 在布局任务完成前保持不可变；
- CPU shaping/rasterization 可以在线程池执行；
- GPU atlas 创建、更新和销毁只能在渲染线程/帧生命周期内执行；
- 不要在渲染线程首次遇到 CJK 字符时同步加载或编译字体资源；
- 字体加载失败、缺字和 fallback 选择必须记录可定位的 WARN/ERROR 日志。

## 验收要求

Text 模块至少覆盖：空文本、非法 UTF-8、缺字 fallback、CJK、换行、裁剪、DPI 改变、atlas 淘汰、字体销毁与正在使用的 layout 生命周期。Renderer2D 的测试只验证 glyph quad 是否正确提交，不再验证字体解析细节。
