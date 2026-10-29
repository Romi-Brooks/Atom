# Atom Scene Schema v1（T2.1）

> 状态：**schema 已冻结**；解析器选型已定（§8：A 自研），实现见 T2.2。
> 更新日期：2026-10-25
> 关联：`Game-Readiness-Assessment-CN.md` 的 T2.1 / T2.2 / T2.3。

本文定义 Atom 的关卡描述格式。T2.1 只做**格式与校验规则**，落地读取是 T2.2，Prefab 是 T2.3。

## 1. 目标与非目标

**目标**：一个可被 `git diff` 读懂、可被人类手写、可被程序严格拒绝的 Scene 文件；加载失败必须能指出"哪个文件、
第几行、哪个实体、哪个字段、为什么"。

**非目标**（v1 明确不做）：任意 C++/Lua 对象序列化、组件反射注册、跨 Scene 引用、嵌套 Prefab、二进制 cook
格式、编辑器的 round-trip 保真。

## 2. 总体约定

| 项 | 决定 |
|---|---|
| 语法 | **JSONC**：标准 JSON + `//` 行注释。**不允许**尾随逗号、块注释 |
| 编码 | UTF-8，无 BOM |
| 单位 | 世界像素、**y 向下**、旋转为弧度且逆时针为正（与 `Renderer2D` / `Camera2D` 一致） |
| 坐标 | 世界原点在 (0,0)，没有"场景偏移"字段；T3.1 的物理在边界处一次性换算米制 |
| 资源引用 | 只能 `res://...`（`AssetPath` 规则），禁止绝对路径、盘符、`..` |
| 版本 | `schema_version` 必填，v1 只接受 `1`；未知版本必须拒绝，不得"尽力解析" |
| 未知字段 | **一律拒绝**（防拼写错误静默生效），错误里给出最接近的合法字段名 |

## 3. 顶层结构

```jsonc
{
  "schema_version": 1,            // 必填，整数，必须等于 1
  "id": "smoke_room",             // 必填，[a-z][a-z0-9_]*，≤ 64，项目内唯一
  "name": "Smoke Room",           // 可选，显示名，≤ 64
  "entities": [ /* ... */ ]       // 必填，数组，可为空
}
```

## 4. 实体

```jsonc
{
  "id": 2,                        // 必填，uint32，1..4294967295，场景内唯一，跨编辑稳定
  "name": "player",               // 可选，≤ 64，只用于调试与诊断
  "tags": ["active"],             // 可选，只允许 "active" / "static"
  "transform": { },               // 可选，缺省为下面的默认值
  "sprite": { },                  // 可选
  "camera": { },                  // 可选
  "lifetime": 1.5                 // 可选，秒；缺省表示没有 Lifetime 组件
}
```

### 4.1 `transform`

| 字段 | 类型 | 默认 | 说明 |
|---|---|---|---|
| `position` | `[number, number]` | `[0, 0]` | 世界像素 |
| `rotation` | `number` | `0` | 弧度，逆时针 |
| `scale` | `[number, number]` | `[1, 1]` | 必须非零（负值合法，用于镜像；0 会拒绝） |

### 4.2 `sprite`

| 字段 | 类型 | 默认 | 说明 |
|---|---|---|---|
| `texture` | `string` | **必填** | `res://` URI |
| `source` | `[number, number, number, number]` | 缺省 = 整张图 | 纹理内的 texel 矩形 `[x, y, w, h]`，`w`/`h` 必须 > 0 |
| `size` | `[number, number]` | 缺省 = `source`，再缺省 = 纹理尺寸 | 世界像素尺寸，必须 > 0 |
| `pivot` | `[number, number]` | `[0.5, 0.5]` | 归一化原点；`[0,0]` 左上，`[1,1]` 右下 |
| `tint` | `[integer ×4]` | `[255,255,255,255]` | 每个通道 0..255 |
| `layer` | `integer` | `0` | 先按 layer 排序，越大越靠前 |
| `sort_key` | `integer` | `0` | 同 layer 内次级键 |
| `flip_x` / `flip_y` | `bool` | `false` | UV 镜像，不改变几何 |

`layer` / `sort_key` / 实体 `id` 共同决定绘制顺序（见 `Render/Scene2D/README-CN.md`）。

### 4.3 `camera`

| 字段 | 类型 | 默认 | 说明 |
|---|---|---|---|
| `zoom` | `number` | `1` | 必须 > 0 |
| `viewport` | `[number, number]` | `[0, 0]` | 像素；`[0,0]` 表示"用渲染目标尺寸" |
| `active` | `bool` | `false` | 一个场景最多一个 `true`（见 §6.3） |

相机位置来自同一实体的 `transform.position`——**没有**独立的相机坐标字段，避免两处真相。

### 4.4 `lifetime`

`number`（秒，≥ 0）。存在即挂 `Lifetime` 组件；`0` 表示下一帧就过期。

### 4.5 `tags`

只允许 `"active"`（参与模拟）与 `"static"`（不被物理移动）。`pending_destroy` 是运行时内部标记，**出现在场景
文件里属于错误**。

## 5. 与 ECS 组件的映射（唯一真相）

| 场景字段 | ECS 组件 | 备注 |
|---|---|---|
| `id` | `SceneEntityId::value` | 实体身份；不是 `Entity` 句柄 |
| `name` | `Name::value`（可选） | 只用于诊断 |
| `tags` | `Active` / `Static` tag | 空组件，presence 即数据 |
| `transform` | `Transform2D` + `RenderTransform2D` | 两者都创建；显示副本初始等于模拟值 |
| `sprite` | `SpriteRenderer` | `texture` → `texture_uri`，其余字段一一对应 |
| `camera` | `Camera2D` | `active` 不落组件，只影响创建顺序（§6.3） |
| `lifetime` | `Lifetime` | |

没有对应组件的字段**不允许**存在。`name` 在 C++ 里是可选组件：加载器应为有 `name` 的实体创建 `Name`，没有就不创建。

## 6. 校验规则

### 6.1 语法与结构

- 只接受 §3/§4 列出的键；任何多余键（含拼写错误）都报错；
- 同一个对象内**重复键**报错（JSON 允许，语义危险）；
- 数字必须是有限值，`NaN` / `Infinity` / `1e999` 报错；
- 整数不能写成 `2.0`（避免"看起来是整数"的浮点）；
- 每个 `id` 必须是 `1..4294967295` 的整数。

### 6.2 语义

- `id` 重复 → 报错并指出两处行号；
- `texture` 不是 `res://` → 报错；
- `scale` 任一维为 0 → 报错（退化矩阵）；
- `sprite.size` / `source` 的 `w`/`h` ≤ 0 → 报错；
- `camera.zoom` ≤ 0 → 报错；
- `tags` 含未知值或 `pending_destroy` → 报错。

### 6.3 相机唯一性

`active: true` 的相机**最多一个**。

- 有一个：加载器先创建它，因此它是激活相机（`FindActiveCamera` 取最低实体索引，这是实现细节而非格式契约）；
- 一个都没有：若场景里只有一个相机，用它；若有多个相机且都未标 `active`，**加载失败**（避免"哪个是相机"靠猜）；
- 多于一个 `active: true`：加载失败。

### 6.4 全有或全无

任何错误都使整次加载失败，不做部分加载、不留半个世界。错误信息必须包含文件 URI，便于在编辑器/日志里跳转。

## 7. 错误诊断规范

两条不同阶段的错误，格式不同但都可定位：

**解析错误（第一个即返回）**

```text
res://scenes/smoke_room.scene.json:12:18: expected ',' or '}' after object member
```

**校验错误（收集，最多 32 条，一次返回）**

```text
res://scenes/smoke_room.scene.json: entity 7 ("crate"): sprite.pivot: expected 2 numbers, got 1
res://scenes/smoke_room.scene.json: entity 9: unknown field "spirte" (did you mean "sprite"?)
res://scenes/smoke_room.scene.json: entity 12: duplicate entity id 7 (first seen at line 24)
```

规则：

1. 每条错误包含 `uri`；
2. 语义错误尽量带 `entity id` 与 `name`；
3. 字段路径用点号从实体根开始（`sprite.pivot`），不写 C++ 类型名；
4. 未知字段给出最接近的合法名（编辑距离 ≤ 2 时）；
5. 同一个实体最多报 8 条，避免一个坏实体刷屏；
6. 不打印无关的展开上下文，日志里一行一条。

## 8. 解析器选型（已定：A 自研）

约束：解析器**不得**出现在 `ECS` / `Render` / `Scene` 的公共 API 里；T2.2 只把解析后的**值**交给建实体的代码。
两个候选：

**A. 自研 JSONC tokenizer（推荐）**
- 位置 `Scene/Json/JsonReader.{hpp,cpp}`，约 500–700 行 + 测试；
- 天然给出 `line:column`，天然支持 `//` 注释，错误文案与 §7 完全一致；
- 无新依赖、编译开销小、可完全离线审计；
- 代价：我们自己维护；需要写清数字/转义/嵌套深度的边界测试（深度上限 64，字符串长度上限）。

**B. 引入 `nlohmann/json` 单头文件**
- 成熟、DOM 好用；
- 但 JSONC 注释要额外预处理（字符串内的 `//` 会误伤），`line:column` 需要自己接 `parse_error` 的 byte 位置
  反查行号，"未知字段 + 建议名"仍然要自己写一层包装；
- 增加约 1 MB 的第三方头，Debug 编译时间明显上升。

**决定：A（2026-10-25 确认）。** 理由与我们当初的判断一致：需求很窄（只读、严格、中文友好定位），自研的实际
代码量比"引入后包装一层"更小，也不给 `ThirdParty/` 增加维护面。

实现落点：
- `Scene/Json/JsonReader.{hpp,cpp}`：tokenizer + 递归下降，`TryParse()` 返回 `SyntaxError{line, column, message}`，
  `Parse()` 是其 `Result` 包装；
- `Scene/SceneSchema.cpp`：把 DOM 变成 `SceneDesc`，收集至多 32 条诊断；
- 若以后需要**写** JSON（存档 T5.5、工具），再补一个约 150 行的 writer，而不是现在预先引入库。

## 9. 示例与测试资源

| 文件 | 用途 |
|---|---|
| `Assets/scenes/smoke_room.scene.json` | 合法样例；T2.2 用它替换 `SeedSmokeScene` |
| `Assets/scenes/Invalid/unknown_field.scene.json` | 拼写错误拒绝 |
| `Assets/scenes/Invalid/duplicate_id.scene.json` | 重复 id 拒绝 |
| `Assets/scenes/Invalid/bad_version.scene.json` | 未知 `schema_version` 拒绝 |

地板阵列不在 v1 手写：几十上百块地砖必须由 T5.1 的 Tilemap 生成，样例只给房间里的少量实体。

## 10. v2 预留（不得在 v1 悄悄加字段）

- `physics` 块（body 类型、shape、sensor）—— 依赖 G0.4 的契约扩展；
- `tilemap` 层（tileset 引用、chunk 坐标）—— T5.1；
- `animation` / `animator` —— T5.3；
- Prefab 嵌套与 override merge —— T2.3 只做平面 Prefab；
- 跨场景引用与 `WorldManifest` —— T5.4。

以上任何一项进入格式都必须 `schema_version` 递增，并保留旧版本读取器的拒绝行为。
