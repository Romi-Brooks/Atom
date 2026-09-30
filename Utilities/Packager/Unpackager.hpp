#ifndef ATOM_UNPACKAGER_HPP
#define ATOM_UNPACKAGER_HPP

// Standard Library
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <memory>

namespace atom::tools {
class Unpackager final {
    public:
        struct Config {
                bool verbose;
                bool preserveStructure;
                bool overwrite;
                std::string outputDir;

                Config();
        };

        struct FileEntry {
                std::string filename;
                uint64_t offset;
                uint64_t size;
                std::string type;
        };

        struct MemoryFile {
                std::string filename;
                std::string type;
                std::vector<char> data;

                [[nodiscard]] auto GetData() const -> const char*;
                [[nodiscard]] auto GetSize() const -> size_t;
                [[nodiscard]] auto ToString() const -> std::string;
        };

        // Operation result
        enum class Result {
            Success,
            ErrorOpenFile,
            ErrorInvalidFormat,
            ErrorReadFileTable,
            ErrorExtractFile,
            ErrorCreateDirectory,
            ErrorEncoding,
            ErrorCorruptedPackage,
            ErrorFilesNotFound,
            ErrorReadData
        };

        Unpackager() = default;
        ~Unpackager();

        Unpackager(const Unpackager&) = delete;
        Unpackager& operator=(const Unpackager&) = delete;
        Unpackager(Unpackager&& other) noexcept;
        Unpackager& operator=(Unpackager&& other) noexcept;

        auto Load(const std::string& packageFile, bool verbose = true) -> Result;

        auto ExtractFileToMemory(const std::string& filename, MemoryFile& memoryFile) -> Result;
        auto ExtractFilesToMemory(const std::vector<std::string>& filenames, std::vector<MemoryFile>& memoryFiles)
            -> Result;
        auto ExtractAllToMemory(std::vector<MemoryFile>& memoryFiles) -> Result;
        auto GetFileData(const std::string& filename, const char** data, size_t* size) -> Result;
        auto ExtractFileToMemory(const std::string& filename) -> std::unique_ptr<MemoryFile>;

        auto UnpackAll(const Config& config = Config()) -> Result;
        auto ExtractFile(const std::string& filename, const Config& config = Config()) -> Result;

        [[nodiscard]] auto GetFileList() const -> std::vector<std::string>;
        [[nodiscard]] auto Contains(const std::string& filename) const -> bool;
        [[nodiscard]] auto GetFileInfo(const std::string& filename) const -> const FileEntry*;
        auto PrintPackageInfo() const -> void;

    private:
        std::vector<FileEntry> file_table_;
        std::unordered_map<std::string, size_t> file_index_;
        std::string package_path_;
        std::ifstream package_stream_;
        // Storage backing GetFileData(); valid until the next GetFileData() call.
        std::vector<char> last_read_buffer_;

        // Utility functions
        auto CreateDirectory(const std::filesystem::path& dir_path) -> bool;
        auto SafePathToString(const std::filesystem::path& path) -> std::string;
        auto GetFileSize(const std::string& filename) -> uint64_t;
        auto ReadFileData(const FileEntry& entry, std::vector<char>& buffer) -> Result;
};
} // namespace atom::tools

#endif // ATOM_UNPACKAGER_HPP
