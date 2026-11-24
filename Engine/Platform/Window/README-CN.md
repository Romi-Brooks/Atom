# Window 模块

`Window/` 是后端无关的宿主门面，位置保持在顶层 `Window/`。

`RenderWindow` 负责窗口配置、事件泵、帧边界和后端选择后的稳定访问面；具体 SDL3 窗口、SDL_GPU 设备和 ImGui glue 分别属于 `Backend/SDL3/` 与 `Backend/SDLGPU/`。因此不要把 `Window/` 移进某个 backend 目录，也不要让 Screen 或游戏系统直接引用 SDL 类型。

`Runtime/Application` 拥有初始化、主循环和关闭时序；游戏侧通常通过 Screen、输入系统与渲染 contract 工作，而不是接管 `RenderWindow` 生命周期。
