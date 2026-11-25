# SDL_GPU 后端目录说明

```text
Device/       SDL_GPU device、窗口/交换链、帧生命周期
Encoding/     命令和 render-pass 编码；当前的 Renderer2D 兼容适配仍在这里
Pipelines/    SDL_GPU shader variant 加载、内置 2D pipeline factory 和 native pipeline 缓存
Resources/    预留给 texture/buffer/sampler/upload 生命周期拆分
Debug/        ImGui 和 GPU 调试 glue
```

`Render/Pipeline` 保存后端无关的 `PipelineDesc`。
本目录的 pipeline 代码负责将它转换为 `SDL_GPUGraphicsPipelineCreateInfo`。
当前内置 Renderer2D 已经通过 `SDLGPU2DPipelineFactory` 集中创建 primitive/post-process 管线，
避免编码器重复管理 shader 生命周期。
