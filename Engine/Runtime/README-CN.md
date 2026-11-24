# Runtime 模块

`Runtime/` 负责游戏进程内的生命周期和帧调度：`Application`、`Screen`/`ScreenManager`、`GameWorld`、固定顺序 system 调度、命令缓冲与游戏事件队列。

`Screen` 是展示状态的生命周期 contract：接收输入事件、执行可变/固定步更新，并通过 `IRenderDevice` 请求呈现。它不实现 `Renderer2D`、文字、sprite 或 GPU backend；这些属于 Render、UI 与未来 Scene2D。`Screen.cpp` 不存在是有意的：该类型只含纯虚/内联生命周期接口。

不要混淆 `Runtime/` 与 `Backend/Composition/`：前者是游戏运行时，后者只负责把选定的具体 backend 注册和装配起来。
