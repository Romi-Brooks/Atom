# Debugger 模块

`Debugger/` 提供运行时共享 ImGui overlay 的诊断界面；它不是插件系统，也不拥有游戏逻辑。

## 目录职责

- `DebugPanel.*`：全部诊断面板的抽象基类及 attach 生命周期。
- `Overlay*`、`PanelLayout.*`、`FontLoader.*`、`DebuggerConfig.hpp`：共享 overlay 的基础设施。
- `Panels/`：可独立挂载的具体面板。当前包括 `LogDebugger`、`JobDebugger`、`ThreadDebugger` 与 `FpsDebugger`。

新增调试窗口时，应在 `Panels/` 下实现一个 `DebugPanel` 子类；仅当需要第三方动态装卸、版本协商或进程外隔离时，才另行设计插件机制。当前四个面板与引擎 ABI、ImGui 上下文及 Runtime 生命周期同编译、同发布，抽成 plugin 只会增加加载、错误与兼容性边界，并不会带来可用的扩展性。
