# Tilemap v1（JSONC）

资源由 `LoadTilemap(IFileSystem, AssetPath)` 读取，使用 `res://` 等有效 AssetPath URI。未知字段、重复层名、重复 Tile ID、重复分块坐标、重复对象 ID、未定义 Tile ID 均报错。

```json
{
  "schema_version": 1,
  "kind": "tilemap",
  "id": "village",
  "projection": "orthogonal",
  "tile_size": [16, 16],
  "chunk_size": 2,
  "tiles": [
    {"id": 1, "texture": "res://textures/terrain.png", "source": [0, 0, 16, 16], "solid": true, "filter": "nearest"}
  ],
  "layers": [
    {"kind": "tiles", "name": "walls", "draw_order": 0, "collision": true,
     "parallax": [1, 1], "chunks": [{"x": 0, "y": 0, "cells": [1, 1, 0, 0]}]},
    {"kind": "objects", "name": "actors", "objects": [
      {"id": 7, "prefab": "res://prefabs/npc.prefab.json", "position": [5, 6], "rotation": 0}
    ]}
  ]
}
```

`tile_size` 是世界像素大小，`chunk_size` 是每边格子数，范围 1–256。`cells` 必须有 `chunk_size²` 个整数，按行排列；0 表示空格。分块坐标允许负数。Tile 的 `source` 是纹理像素矩形；`solid`、`collision` 缺省为 false，`filter` 缺省 nearest，`draw_order` 缺省 0，`parallax` 缺省 `[1,1]`，对象的 `rotation` 缺省 0。只有 `collision: true` 的图层中 `solid: true` 的格子会产生碰撞体。对象 ID 在整张地图内唯一；游戏负责将 prefab URI 映射到实际实体。

当前只接受 `orthogonal` 投影，其他投影不会被当作正交地图误读。示例文件位于 `Test/World2D/Fixtures/sample.tilemap.json`，CI 测试会通过真实文件系统读取它。
