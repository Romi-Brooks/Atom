[English](Packager.md) | [中文](Packager-CN.md)

***

# Atom 资源打包工具

一个命令行工具，用于将游戏资源文件打包/解包为自定义存档格式（APKG）。位于 [`Utilities/Packager/`](../) 目录。

***

## 格式概述

| 字段  | 值                |
| --- | ---------------- |
| 魔数  | `APKG`           |
| 版本  | `1`              |
| 扩展名 | `.dat`（或其他自定义名称） |

APKG 格式将文件顺序存储，并在存档末尾附加文件表，便于快速查找。

***

## 工具使用

### 运行

```bash
./packager_tool
```

将显示交互式菜单：

```
================================================
Atom Resource Package / Unpackage Tools v1.0
================================================

Operations:
1. Pack from multiple target folders
2. Extract to the extract folder
0. Exit
Input options (0-2):
```

### 打包（选项 1）

1. 输入目录路径，多个目录用逗号分隔 — 例如 `resources/, audio/, textures/`
2. 输入输出包文件名 — 例如 `media_res.dat`
3. 工具将递归收集所有目录中的文件，打包为单个存档文件。

### 解包（选项 2）

1. 输入包文件名 — 例如 `media_res.dat`
2. 所有文件将解包到 `extract/` 目录，保持原始目录结构。

***

## API 参考

### Packager（打包器） — `Packager.hpp`

```cpp
namespace atom::tools {

class Packager {
public:
    struct Config {
        bool compress;           // （预留）启用压缩
        bool verbose;            // 打印详细日志
        bool preserveStructure;  // 在存档中保留目录结构
        bool overwrite;          // 覆盖已存在的输出文件
    };

    enum class Result { SUCCESS, ERROR_OPEN_OUTPUT, ERROR_READ_FILE, ... };

    auto Pack(const std::vector<std::string>& resourcePaths,
              const std::string& outputFile,
              const Config& config = Config()) -> Result;

    [[nodiscard]] auto GetPackedFiles() const -> std::vector<std::string>;
    auto PrintPackageInfo() const -> void;
};

}
```

### Unpackager（解包器） — `Unpackager.hpp`

```cpp
namespace atom::tools {

class Unpackager {
public:
    struct FileEntry {
        std::string filename;
        uint64_t offset;
        uint64_t size;
        std::string type;
    };

    struct Config {
        bool verbose;
        bool preserveStructure;
        bool overwrite;
        std::string outputDir;
    };

    struct MemoryFile {
        std::string filename;
        std::string type;
        std::vector<char> data;
        [[nodiscard]] auto GetData() const -> const char*;
        [[nodiscard]] auto GetSize() const -> size_t;
        [[nodiscard]] auto ToString() const -> std::string;
    };

    enum class Result { SUCCESS, ERROR_OPEN_FILE, ERROR_INVALID_FORMAT, ... };

    auto Load(const std::string& packageFile, bool verbose = true) -> Result;

    // 解包到磁盘
    auto UnpackAll(const Config& config = Config()) -> Result;
    auto ExtractFile(const std::string& filename, const Config& config = Config()) -> Result;

    // 解包到内存
    auto ExtractFileToMemory(const std::string& filename) -> std::unique_ptr<MemoryFile>;
    auto ExtractFileToMemory(const std::string& filename, MemoryFile& memoryFile) -> Result;
    auto ExtractFilesToMemory(const std::vector<std::string>& filenames,
                              std::vector<MemoryFile>& memoryFiles) -> Result;
    auto ExtractAllToMemory(std::vector<MemoryFile>& memoryFiles) -> Result;
    auto GetFileData(const std::string& filename, const char** data, size_t* size) -> Result;

    // 查询
    [[nodiscard]] auto GetFileList() const -> std::vector<std::string>;
    [[nodiscard]] auto Contains(const std::string& filename) const -> bool;
    [[nodiscard]] auto GetFileInfo(const std::string& filename) const -> const FileEntry*;
    auto PrintPackageInfo() const -> void;
};

}
```

### 示例：编程方式打包

```cpp
#include <Packager.hpp>

auto main() -> int {
    atom::tools::Packager packer;
    atom::tools::Packager::Config config;
    config.verbose = true;
    config.preserveStructure = true;

    std::vector<std::string> dirs = {"resources/", "audio/"};
    auto result = packer.Pack(dirs, "game_data.dat", config);

    if (result == atom::tools::Packager::Result::SUCCESS) {
        packer.PrintPackageInfo();
    }
    return 0;
}
```

### 示例：编程方式解包

```cpp
#include <Unpackager.hpp>

auto main() -> int {
    atom::tools::Unpackager unpacker;
    if (unpacker.Load("game_data.dat") == atom::tools::Unpackager::Result::SUCCESS) {
        // 全部解包到内存
        std::vector<atom::tools::Unpackager::MemoryFile> files;
        unpacker.ExtractAllToMemory(files);

        // 或解包到磁盘
        atom::tools::Unpackager::Config config;
        config.outputDir = "extract/";
        config.preserveStructure = true;
        unpacker.UnpackAll(config);
    }
    return 0;
}
```

***

