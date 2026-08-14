# 源码文件标头迁移清单

> 状态：待统一处理
>
> 建立日期：2026-08-14
>
> 范围：仅 Atom 自有源码，不修改 `ThirdParty/` 子模块

## 目标

将旧源码标头中的固定年份和 `All rights reserved` 统一迁移为简洁的 MIT
许可证标识。该任务只修改注释，不应改变代码、格式或行为。

推荐模板：

```cpp
// Copyright (c) YYYY Author
// SPDX-License-Identifier: MIT

/**
 * @file FileName.hpp
 * @brief One-line description.
 */
```

规则：

- `YYYY` 使用文件首次加入仓库的年份；无法可靠确认时可使用当前版权年份范围。
- 保留真实作者信息，不从 Git 历史无法证明的内容中推测作者。
- 删除 `All rights reserved`。
- 每个 Atom 自有 `.h`、`.hpp`、`.c`、`.cpp` 文件最终只保留一个
  `SPDX-License-Identifier: MIT`。
- `@author`、`@date` 和空的 `@attention` 不再强制；Git 历史负责记录作者与时间。
- 上游子模块维持各自许可证和文件标头，不得批量改写。

## 当前状态

审计时共有 95 个 Atom 自有 C/C++ 文件：

- 38 个文件包含旧的 `All rights reserved`。
- 0 个文件包含 SPDX 标识。
- 其余文件没有统一许可证标头。

重新获取实时清单：

```powershell
rg -l "All rights reserved" -g "*.h" -g "*.hpp" -g "*.c" -g "*.cpp" -g "!ThirdParty/**"
rg -L "SPDX-License-Identifier: MIT" -g "*.h" -g "*.hpp" -g "*.c" -g "*.cpp" -g "!ThirdParty/**"
```

## 验收

```powershell
rg "All rights reserved" -g "*.h" -g "*.hpp" -g "*.c" -g "*.cpp" -g "!ThirdParty/**"
rg -l "SPDX-License-Identifier: MIT" -g "*.h" -g "*.hpp" -g "*.c" -g "*.cpp" -g "!ThirdParty/**"
```

第一条命令应无输出；第二条命令应覆盖全部 Atom 自有源码文件。迁移应作为单独的
机械提交完成，便于审查并避免和功能改动混在一起。
