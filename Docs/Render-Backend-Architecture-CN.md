# Atom 渲染后端架构与演进路线

> 状态：已接受的方向与实施基线
>
> 更新日期：2026-09-03
>
> 适用范围：SDL_GPU 首个后端、原生 Vulkan 后端、未来多后端共存

## 1. 已确定的决策

1. Atom 的首个正式渲染后端使用 SDL_GPU。
2. 不保留现有 SDL_Renderer 实现作为正式兼容后端；迁移完成后删除 `SDL_Renderer`、`SDL_Texture` 和 `imgui_impl_sdlrenderer3` 渲染路径。
3. SDL_GPU 使用的原生 API 由 SDL 按平台和设备选择。Atom 初期不分别维护 D3D12、Vulkan、Metal 选择逻辑，也不承诺 D3D11 后端。
4. SDL 继续承担窗口、输入、计时和平台事件；窗口层与渲染设备层必须分离。
5. 第一版 RHI 同时支持 2D 与基础 3D。2D 是首个完整产品路径，3D 从第一阶段就以最小闭环验证接口，不等到 2D 完成后再改造底层。
6. 原生 Vulkan 是第二个正式后端。在 SDL_GPU 后端和 RHI 稳定前，不同时实现 D3D12 或 Metal 原生后端。
7. Shader 采用离线编译。首选 HLSL 作为统一源语言，生成 SDL_GPU 所需的 SPIR-V、DXIL、MSL/metallib；原生 Vulkan 消费同源生成的 SPIR-V。

## 2. 为什么不保留 SDL_Renderer

SDL_Renderer 的价值是用少量 API 快速完成 2D 图形，但 Atom 已决定建设支持自定义 Shader、Pipeline、GPU Buffer、Render Pass、Compute 和 3D 的渲染器。继续保留它会产生两套资源类型、两套 ImGui renderer、两套批处理逻辑以及不同的功能上限。

它不再作为运行时 fallback。GPU 初始化失败应返回清晰错误并记录设备、驱动和 Shader 格式信息，而不是静默切换到行为不同的渲染路径。需要无 GPU 测试时，应实现不创建真实图形资源的 `NullRenderDevice`，而不是使用 SDL_Renderer 模拟正式后端。

旧实现只在迁移期间短暂存在，用于逐项对照现有窗口、纹理和示例行为；当 SDL_GPU 达到迁移验收条件后一次性删除。

## 3. 总体分层

```text
Game / Screen / UI
        |
Renderer2D        Renderer3D
        \          /
         Render Scene / Queue
                 |
          Atom RHI contracts
                 |
       +---------+----------+
       |                    |
 SDL_GPU backend     Native Vulkan backend
       |                    |
 D3D12/Vulkan/Metal       Vulkan API
       +---------+----------+
                 |
          SDL window/events
```

### 3.1 平台层

平台层负责：

- 窗口创建、销毁和像素尺寸；
- 输入与窗口事件；
- 高精度计时；
- 为渲染后端提供受控的原生窗口访问；
- 不包含 `Clear()`、`DrawTexture()` 或 `Present()` 等绘制职责。

现有 `IRenderWindow : IRenderTarget` 必须拆分。建议将窗口契约改为 `IWindow`，由渲染后端单独持有 device 和 swapchain。

### 3.2 RHI 层

第一版只抽象已经由 SDL_GPU 和 Vulkan 共同证明需要的概念：

- `IRenderDevice`
- `ISwapchain`
- `ICommandBuffer`
- `IRenderPassEncoder`
- `IGraphicsPipeline` / `IComputePipeline`
- `IShader`
- `IBuffer`
- `ITexture`
- `ISampler`
- `IFence`

公共描述结构包括 Buffer、Texture、Sampler、Shader、VertexLayout、Blend、Raster、DepthStencil、RenderTarget 和 Pipeline 描述。公共头文件不得暴露 `SDL_GPU*`、`Vk*`、D3D 或 Metal 类型。

不要为了覆盖 Vulkan 的全部能力而提前设计庞大的通用接口。SDL_GPU 暂不提供、且首期不需要的能力，应通过 capability 或未来扩展接口加入，不能先制造空抽象。

### 3.3 高层渲染器

`Renderer2D` 负责 sprite、矩形、圆形、文字、图集、camera、layer、裁剪、blend 和 batching。现有 `IRenderTarget` 的便捷绘制能力迁移到这里，不再作为后端契约。

`Renderer3D` 首期只负责：

- perspective/orthographic camera；
- vertex/index mesh；
- model/view/projection 数据；
- depth buffer 与 depth test；
- 一个基础材质和纹理；
- 最小 opaque pass。

PBR、阴影、骨骼动画、延迟渲染、Render Graph、光线追踪和多队列异步计算均不属于首期。

## 4. SDL_GPU 后端

建议目录：

```text
Backend/SDL3/Window/          SDL 窗口与事件
Backend/SDLGPU/              SDL_GPU 的 RHI 实现
Render/RHI/                   后端无关契约与描述结构
Render/Renderer2D/            高层 2D 渲染
Render/Renderer3D/            高层 3D 渲染
Render/Shader/                Shader 源码、清单与加载器
```

初始化顺序：

1. 初始化 SDL Video/Events。
2. 创建普通可调整尺寸的 SDL Window。
3. 使用可提供的 Shader 格式创建 `SDL_GPUDevice`，让 SDL 自动选择原生 driver。
4. 调用 `SDL_ClaimWindowForGPUDevice()`。
5. 查询并记录实际 driver、设备名、支持的 Shader 格式和 swapchain 格式。
6. 创建默认 sampler、错误材质和基础 pipeline。

每帧顺序：

1. 获取 command buffer；
2. 获取 swapchain texture；
3. 上传本帧动态数据；
4. 开始 render pass；
5. 提交 3D opaque、2D、UI、Debugger；
6. 结束 pass；
7. 提交 command buffer。

窗口最小化或 swapchain texture 暂时不可获取时跳过绘制，不忙等。Resize 事件只更新尺寸状态，实际后端资源变更在安全的帧边界完成。

## 5. Shader 与材质工作流

```text
Render/Shader/Source/*.hlsl
          |
          +-- SPIR-V ------ SDL_GPU Vulkan / Native Vulkan
          +-- DXIL -------- SDL_GPU D3D12
          +-- MSL/metallib  SDL_GPU Metal
```

实施要求：

- Shader 编译由 CMake custom command 或独立 asset build 工具执行；
- 构建产物按后端格式和 build configuration 存放，不手工提交临时文件；
- Shader manifest 记录 stage、entry point、资源绑定和变体；
- 优先通过 reflection 生成绑定元数据，避免手写资源数量与槽位；
- Pipeline cache key 至少包含 Shader、vertex layout、blend、raster、depth/stencil 和 render-target formats；
- Debug 构建支持重新加载已编译二进制，源码编译热重载延后；
- 资源绑定约定必须在第一个 Shader 合入前固定并写入测试。

建议的首批 Shader：

- `Triangle.hlsl`：验证 vertex/fragment pipeline；
- `Sprite.hlsl`：纹理、sampler、颜色和矩阵；
- `Primitive.hlsl`：基础 2D/调试图元；
- `MeshUnlit.hlsl`：基础 3D、深度和相机。

## 6. 原生 Vulkan 后端

原生 Vulkan 后端复用 SDL 窗口与事件，但不复用 SDL_GPU device。SDL 只提供 Vulkan instance extensions 和 `VkSurfaceKHR` 创建入口。

建议目录：

```text
Backend/Vulkan/
  VulkanRenderDevice.*
  VulkanSwapchain.*
  VulkanCommandBuffer.*
  VulkanPipeline.*
  VulkanResources.*
  VulkanDescriptors.*
```

第二后端进入条件：

- SDL_GPU 已完成 2D textured quad 和基础 3D mesh；
- RHI 公共接口至少经过两个实际场景验证；
- Shader manifest 能稳定生成 SPIR-V；
- 资源所有权、延迟销毁和帧同步规则已有测试；
- RHI 中没有 SDL 类型泄漏到上层。

Vulkan 后端第一阶段只实现与 SDL_GPU 已使用能力等价的子集。不要以 Vulkan 的额外能力反向污染公共 RHI；确实需要的高级功能通过 capability 检测和明确扩展接口暴露。

## 7. 多后端选择与生命周期

渲染后端 ID 定义为：

- `sdl_gpu`：默认正式后端；
- `vulkan`：后续原生 Vulkan 后端；
- `null`：可选的无 GPU 测试后端。

后端选择发生在创建窗口和 RenderDevice 之前，一个进程运行期间不支持热切换。GPU 资源属于创建它的 device，禁止跨后端共享；切换后端需要完整销毁渲染系统并重新加载 GPU 资源。

现有只返回 `IRenderWindow` 的 `RenderBackendRegistry::WindowFactory` 应替换为创建完整渲染系统的 factory。推荐结果至少包含 window、device 和 swapchain，或由一个 `IRenderBackend` 协调三者。

## 8. 2D 与 3D 的实施原则

现在就支持基础 3D是必要的，但“支持 3D”不等于立即建设完整 3D 引擎。

如果先把 RHI 写成纯 2D 接口，再增加 3D，最容易产生的历史包袱是：没有 depth attachment、没有 vertex/index buffer、没有多 stage Shader、固定正交坐标、Pipeline 状态不足、Texture 与 render target 混淆。因此第一版必须用一个真实 3D mesh 验证这些能力。

另一方面，如果第一版就加入场景图、PBR、阴影、光照管理和 Render Graph，会在资源生命周期、Shader 约定和后端行为尚未稳定时扩大返工面。因此采用“双验证、单主线”：RHI 同时用 2D sprite 和 3D unlit mesh 验证，功能建设先以 Renderer2D 为主。

## 9. 分阶段计划与验收条件

### 阶段 A：拆分与最小设备

- [ ] 拆分 `IWindow` 与渲染职责。
- [ ] 建立最小 RHI 描述和 RAII resource wrapper。
- [ ] 创建 SDL_GPU device、claim window、清屏并提交空帧。
- [ ] 记录实际 GPU driver 和错误诊断信息。

验收：窗口可创建、调整尺寸、最小化和关闭；验证层无生命周期错误。

### 阶段 B：Shader 与 2D/3D 双验证

- [ ] 建立 HLSL 离线编译与 Shader manifest。
- [ ] 绘制自定义 Shader 三角形。
- [ ] 绘制带纹理的 2D quad。
- [ ] 绘制带 depth test 的 3D unlit mesh。

验收：Windows 上 SDL 自动选择的 driver 能完成全部示例；至少在 Vulkan 路径上用 RenderDoc 检查一帧。

### 阶段 C：迁移现有功能

- [ ] 实现 Renderer2D batching 和现有图元能力。
- [ ] 将图片解码与 GPU 上传分离。
- [ ] 接入 `imgui_impl_sdlgpu3`。
- [ ] 迁移现有窗口、Layout 和 Debugger 示例。
- [ ] 删除 SDL_Renderer、SDL_Texture 和 `imgui_impl_sdlrenderer3` 路径。

验收：现有可视示例在 SDL_GPU 上达到功能等价，代码库不再创建 `SDL_Renderer`。

### 阶段 D：原生 Vulkan 与多后端验证

- [ ] 实现 Vulkan instance、device、surface、swapchain 与同步。
- [ ] 复用相同 Shader manifest、Renderer2D 和 Renderer3D。
- [ ] 实现 Vulkan ImGui renderer 接入。
- [ ] 对 SDL_GPU/Vulkan 做截图一致性、resize 和资源生命周期测试。

验收：同一上层示例只修改后端 ID 即可运行，两种后端不需要条件编译游戏代码。

## 10. 官方参考

- SDL GPU API：https://wiki.libsdl.org/SDL3/CategoryGPU
- SDL GPU Shader formats：https://wiki.libsdl.org/SDL3/SDL_GPUShaderFormat
- SDL ShaderCross：https://github.com/libsdl-org/SDL_shadercross
- SDL Vulkan integration：https://wiki.libsdl.org/SDL3/CategoryVulkan
