[English](README.md) | [中文](README-CN.md)

***

# Atom Resource Packager

A command-line tool for packing and unpacking game resource files into a custom archive format (HPKG). Located under [`Utilities/Packager/`](../../Utilities/Packager/).

***

## Format Overview

| Field        | Value                       |
| ------------ | --------------------------- |
| Magic Number | `HPKG`                      |
| Version      | `1`                         |
| Extension    | `.dat` (or any custom name) |

The HPKG format stores files sequentially with a file table appended at the end of the archive for fast lookup.

***

## Tool Usage

### Running

```bash
./packager_tool
```

An interactive menu will appear:

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

### Packing (Option 1)

1. Enter directories separated by commas — e.g. `resources/, audio/, textures/`
2. Enter output package name — e.g. `media_res.dat`
3. The tool recursively collects all files from the directories and packs them into a single archive.

### Unpacking (Option 2)

1. Enter the package file name — e.g. `media_res.dat`
2. All files are extracted to the `extract/` directory, preserving the original directory structure.

***

## API Reference

### Packager (`Packager.hpp`)

```cpp
namespace atom::tools {

class Packager {
    struct Config {
        bool compress;           // (reserved) enable compression
        bool verbose;            // print detailed logs
        bool preserveStructure;  // preserve directory structure in archive
        bool overwrite;          // overwrite existing output file
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

### Unpackager (`Unpackager.hpp`)

```cpp
namespace atom::tools {

class Unpackager {
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

    // Extract to disk
    auto UnpackAll(const Config& config = Config()) -> Result;
    auto ExtractFile(const std::string& filename, const Config& config = Config()) -> Result;

    // Extract to memory
    auto ExtractFileToMemory(const std::string& filename) -> std::unique_ptr<MemoryFile>;
    auto ExtractAllToMemory(std::vector<MemoryFile>& memoryFiles) -> Result;

    // Query
    [[nodiscard]] auto GetFileList() const -> std::vector<std::string>;
    [[nodiscard]] auto Contains(const std::string& filename) const -> bool;
    auto PrintPackageInfo() const -> void;
};

}
```

### Example: Packing Programmatically

```cpp
#include <Packager/Packager.hpp>

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

### Example: Unpacking Programmatically

```cpp
#include <Packager/Unpackager.hpp>

auto main() -> int {
    atom::tools::Unpackager unpacker;
    if (unpacker.Load("game_data.dat") == atom::tools::Unpackager::Result::SUCCESS) {
        // Extract all to memory
        std::vector<atom::tools::Unpackager::MemoryFile> files;
        unpacker.ExtractAllToMemory(files);

        // Or extract to disk
        atom::tools::Unpackager::Config config;
        config.outputDir = "extract/";
        config.preserveStructure = true;
        unpacker.UnpackAll(config);
    }
    return 0;
}
```

***

