# Vulkan 渲染后端 — 架构规划

> 目标版本：Beta 1.0
> VSDK 路径：`D:\Program\Vulkan SDK`

---

## 1. 当前架构

```
Window/App
    │
    ▼
SDL3RenderWindow  ←── 实现 ──→  IRenderWindow / IRenderTarget
    │
    ├── SDL_Window*      (窗口管理、事件)
    └── SDL_Renderer*    (2D 渲染 — 底层 D3D11/OpenGL)
    │
    ├── SDL_RenderClear / SDL_RenderTexture / SDL_RenderFillRect  ← 即时模式
    └── SDL_Texture*    (纹理)

SDL3Texture  ←── 实现 ──→  ITexture
    └── SDL_Texture*
```

**瓶颈：** `SDL_Renderer` 是固定管线 2D API，无法对接 Vulkan 的显式 GPU 管线。

---

## 2. 目标架构

```
Window/App (不关心后端)
    │
    ▼
RenderWindow (窗口 + 渲染聚合)
    │
    ├── IWindow          ← 窗口管理 (SDL3 负责)
    │
    └── IRenderer        ← 渲染抽象
            │
            ├── [SDL3_RendererBackend]  (当前默认 — 保留作为 fallback)
            │       └── SDL_Renderer
            │
            └── [Vulkan_RendererBackend] (Beta 目标)
                    └── VkDevice / VkSwapchain / VkPipeline
```

### 2.1 新增接口层

```cpp
// Engine/Interfaces/IRenderer.hpp
class IRenderer {
public:
    virtual ~IRenderer() = default;

    // 生命周期
    virtual auto Initialize(SDL_Window* window) -> bool = 0;
    virtual auto Shutdown() -> void = 0;

    // 帧操作
    virtual auto BeginFrame() -> void = 0;
    virtual auto EndFrame() -> void = 0;
    virtual auto Clear(const Color& color) -> void = 0;

    // 2D 绘制 (上层不关心后端实现)
    virtual auto DrawTexture(ITexture& tex, float x, float y) -> void = 0;
    virtual auto DrawRect(float x, float y, float w, float h, const Color& color) -> void = 0;
    virtual auto DrawCircle(float cx, float cy, float radius, const Color& color) -> void = 0;

    // ImGui 需要原生句柄
    [[nodiscard]] virtual auto GetNativeHandle() const -> void* = 0;
};
```

```cpp
// Engine/Interfaces/ITexture.hpp — 后端无关纹理接口
class ITexture {
public:
    virtual ~ITexture() = default;
    virtual auto Load(const std::string& path) -> bool = 0;
    virtual auto GetSize() const -> Vec2 = 0;
};
```

### 2.2 SDL3_RendererBackend（保持现状）

- 包装现有 `SDL_Renderer*` 代码
- 负责 `SDL_CreateRenderer`、`SDL_RenderTexture` 等
- 保留作为默认后端 + 回退方案

### 2.3 Vulkan_RendererBackend（Beta 目标）

使用用户 Vulkan SDK 实现 `IRenderer`：

```
Vulkan_RendererBackend
├── VkInstance / VkDevice
├── VkSwapchainKHR         ← 从 SDL_Window 创建
├── VkRenderPass / VkPipeline    ← 内置 2D 管线
├── VkCommandBuffer        ← 每帧记录绘制命令
├── VkBuffer (顶点)         ← 全屏四边形 / 精灵
├── VkDescriptorSet        ← 纹理采样
└── VkShaderModule         ← 编译内置 SPIR-V shader
```

**关键设计决策：**

| 决策 | 选择 | 理由 |
|------|------|------|
| Shader 管理 | 内置 SPIR-V（编译为 C 数组） | 避免运行时加载外部文件 |
| 纹理上传 | staging buffer + VkImage | 标准 Vulkan 做法 |
| 2D 几何 | 单一全屏 quad + 动态顶点缓冲区 | 简化管线数量 |
| 窗口 Surface | `SDL_Vulkan_CreateSurface` | SDL3 原生支持 |

---

## 3. 实施步骤

### Phase 1 — 抽象现有渲染器（2 周）

- [ ] 提取 `IRenderer` / `ITexture` 接口
- [ ] 将 `SDL3RenderWindow` 拆分为 `Window` + `Renderer`
- [ ] 实现 `SDL3_RendererBackend`（包装现有代码）
- [ ] 验证所有示例正常工作

### Phase 2 — Vulkan 后端最小实现（3 周）

- [ ] CMake 集成 Vulkan SDK（`find_package(Vulkan)` + `D:\Program\Vulkan SDK`）
- [ ] `Vulkan_RendererBackend` 框架：Instance → Device → Swapchain
- [ ] 内置 2D 着色器（vert+frag SPIR-V）
- [ ] Clear + Present（显示纯色背景）
- [ ] `DrawTexture`（纹理上屏）
- [ ] `DrawRect` / `DrawCircle`（简单几何）

### Phase 3 — ImGui + 完善（1 周）

- [ ] ImGui Vulkan 集成（`imgui_impl_vulkan`）
- [ ] 窗口 Resize 处理（Swapchain 重建）
- [ ] 错误恢复（设备丢失等）

### Phase 4 — 优化 + 双后端并行维护（1 周）

- [ ] 运行时后端选择（CMake 选项 / 启动参数）
- [ ] 性能比较
- [ ] 文档更新

---

## 4. CMake 改动

```cmake
option(ATOM_USE_VULKAN "Enable Vulkan render backend" OFF)

if(ATOM_USE_VULKAN)
    set(VULKAN_SDK "D:/Program/Vulkan SDK")
    find_package(Vulkan REQUIRED)
    # 编译 Vulkan_RendererBackend
    # 链接 vulkan-1.lib + shaderc (编译 SPIR-V)
endif()

# SDL3_RendererBackend 始终编译（作为 fallback）
```

---

## 5. 对现有代码的影响

| 组件 | 影响 | 说明 |
|------|------|------|
| `SDL3RenderWindow` | **重构** | 拆分为 Window + Renderer |
| `SDL3Texture` | **重构** | 由具体类改为通过 Renderer 创建 |
| `Debugger` | **低** | `GetNativeRendererHandle()` 仍然有效 |
| `ImGui 集成` | **中** | 需添加 `imgui_impl_vulkan` 后端 |
| 所有 `Draw*` 调用 | **无** | 调用方走 IRenderer 接口 |
| 示例程序 | **无** | 不需要改动 |

---

## 6. 不做的

- **3D 渲染支持** — 引擎是 2D 引擎，Vulkan 后端只负责 2D 绘制
- **着色器热重载** — Beta 版本不需要
- **多队列 / 异步计算** — 2D 渲染用单队列足够
- **光线追踪** — 无意义
