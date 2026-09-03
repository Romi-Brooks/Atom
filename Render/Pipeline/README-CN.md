# Render Pipeline

这里放后端无关的 pipeline 描述和材质布局，不放 `SDL_GPUGraphicsPipeline*`、`VkPipeline` 或其他 native 指针。

`GraphicsPipelineDesc` 表达上层需要的顶点布局、blend、raster 和 depth 状态。具体后端在自己的 `PipelineCache` 中把它转换为 native pipeline，并按描述缓存。Renderer2D 和 Renderer3D 共用这套描述；它们只提供不同的 vertex layout 和 shader program。

推荐依赖方向：

```text
Renderer2D / Renderer3D
        ↓
Render/Pipeline
        ↓
Backend/Contracts/Render
        ↓
Backend/SDLGPU 或 Backend/Vulkan
```

不要在本目录添加后端专用枚举（例如 `ChromaticAberration`）或直接保存 SDL_GPU/Vulkan 对象。
