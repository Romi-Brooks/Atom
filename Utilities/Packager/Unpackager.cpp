// Self Dependency
#include "Unpackager.hpp"

// Standard Library
#include <iostream>

// Third party Library
#include <utf8.h>

namespace fs = std::filesystem;

namespace atom::tools {
Unpackager::Config::Config() : verbose(false), preserveStructure(true), overwrite(true), outputDir(".") {}

auto Unpackager::MemoryFile::GetData() const -> const char* {
    return data.data();
}

auto Unpackager::MemoryFile::GetSize() const -> size_t {
    return data.size();
}

auto Unpackager::MemoryFile::ToString() const -> std::string {
    return {data.begin(), data.end()};
}

Unpackager::~Unpackager() {
    if (package_stream_.is_open()) {
        package_stream_.close();
    }
}

Unpackager::Unpackager(Unpackager&& other) noexcept
    : file_table_(std::move(other.file_table_)), file_index_(std::move(other.file_index_)),
      package_path_(std::move(other.package_path_)) {
    // Stream object movement is complex; new object reopens as needed
    // 流对象移动复杂，新对象按需重新打开
}

Unpackager& Unpackager::operator=(Unpackager&& other) noexcept {
    if (this != &other) {
        file_table_ = std::move(other.file_table_);
        file_index_ = std::move(other.file_index_);
        package_path_ = std::move(other.package_path_);
        if (package_stream_.is_open()) {
            package_stream_.close();
        }
    }
    return *this;
}

auto Unpackager::CreateDirectory(const fs::path& dir_path) -> bool {
    try {
        if (!fs::exists(dir_path)) {
            return fs::create_directories(dir_path);
        }
        return true;
    } catch (const std::exception& e) {
        std::cout << "错误: 创建目录失败 " << dir_path << ": " << e.what() << std::endl;
        return false;
    }
}

auto Unpackager::SafePathToString(const fs::path& path) -> std::string {
    try {
        return path.string();
    } catch (const std::exception& e) {
        std::cout << "警告: 路径转换错误: " << e.what() << std::endl;
        return "unknown_path";
    }
}

auto Unpackager::GetFileSize(const std::string& filename) -> uint64_t {
    try {
        return static_cast<uint64_t>(fs::file_size(filename));
    } catch (...) {
        return 0;
    }
}

auto Unpackager::ReadFileData(const FileEntry& entry, std::vector<char>& buffer) -> Result {
    if (!package_stream_.is_open()) {
        package_stream_.open(package_path_, std::ios::binary);
        if (!package_stream_.is_open()) {
            return Result::ERROR_OPEN_FILE;
        }
    }

    package_stream_.clear();
    package_stream_.seekg(static_cast<std::streamoff>(entry.offset));
    if (!package_stream_) {
        return Result::ERROR_READ_DATA;
    }

    buffer.resize(entry.size);
    package_stream_.read(buffer.data(), static_cast<long long>(entry.size));

    if (!package_stream_) {
        return Result::ERROR_READ_DATA;
    }

    return Result::SUCCESS;
}

auto Unpackager::Load(const std::string& packageFile, bool verbose) -> Result {
    package_path_ = packageFile;

    if (package_stream_.is_open()) {
        package_stream_.close();
    }

    if (!fs::exists(packageFile)) {
        if (verbose)
            std::cout << "错误: 包文件不存在: " << packageFile << std::endl;
        return Result::ERROR_OPEN_FILE;
    }

    uint64_t package_size = GetFileSize(packageFile);
    if (verbose)
        std::cout << "包文件大小: " << package_size << " 字节" << std::endl;

    if (package_size < 20) {
        if (verbose)
            std::cout << "错误: 文件太小，不是有效的包文件" << std::endl;
        return Result::ERROR_INVALID_FORMAT;
    }

    std::ifstream input(packageFile, std::ios::binary);
    if (!input.is_open()) {
        if (verbose)
            std::cout << "错误: 无法打开包文件" << std::endl;
        return Result::ERROR_OPEN_FILE;
    }

    // Read magic number
    // 读取魔数
    char magic[4];
    input.read(magic, 4);
    if (!input) {
        if (verbose)
            std::cout << "错误: 读取文件头失败" << std::endl;
        input.close();
        return Result::ERROR_INVALID_FORMAT;
    }

    std::string magic_str(magic, 4);
    if (verbose)
        std::cout << "文件魔数: " << magic_str << std::endl;

    if (!std::equal(std::begin(MAGIC), std::end(MAGIC), magic)) {
        if (verbose)
            std::cout << "错误: 无效的文件魔数，期望 'APKG'" << std::endl;
        input.close();
        return Result::ERROR_INVALID_FORMAT;
    }

    // Read version number
    // 读取版本号
    uint16_t version;
    input.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (!input) {
        if (verbose)
            std::cout << "错误: 读取版本号失败" << std::endl;
        input.close();
        return Result::ERROR_INVALID_FORMAT;
    }

    if (verbose)
        std::cout << "文件版本: " << version << std::endl;
    if (version != VERSION) {
        if (verbose)
            std::cout << "错误: 不支持的版本号: " << version << std::endl;
        input.close();
        return Result::ERROR_INVALID_FORMAT;
    }

    // Read file count
    // 读取文件数量
    uint32_t file_count;
    input.read(reinterpret_cast<char*>(&file_count), sizeof(file_count));
    if (!input) {
        if (verbose)
            std::cout << "错误: 读取文件数量失败" << std::endl;
        input.close();
        return Result::ERROR_INVALID_FORMAT;
    }

    if (verbose)
        std::cout << "文件数量: " << file_count << std::endl;
    if (file_count > 1000000) {
        if (verbose)
            std::cout << "错误: 文件数量异常: " << file_count << std::endl;
        input.close();
        return Result::ERROR_CORRUPTED_PACKAGE;
    }

    // Read file table offset
    // 读取文件表偏移量
    input.seekg(-static_cast<std::streamoff>(sizeof(uint64_t)), std::ios::end);
    if (!input) {
        if (verbose)
            std::cout << "错误: 定位到文件表偏移量失败" << std::endl;
        input.close();
        return Result::ERROR_INVALID_FORMAT;
    }

    uint64_t table_offset;
    input.read(reinterpret_cast<char*>(&table_offset), sizeof(table_offset));
    if (!input) {
        if (verbose)
            std::cout << "错误: 读取文件表偏移量失败" << std::endl;
        input.close();
        return Result::ERROR_INVALID_FORMAT;
    }

    if (verbose)
        std::cout << "文件表偏移量: " << table_offset << std::endl;
    if (table_offset >= package_size || table_offset < 10) {
        if (verbose)
            std::cout << "错误: 无效的文件表偏移量: " << table_offset << std::endl;
        input.close();
        return Result::ERROR_CORRUPTED_PACKAGE;
    }

    // Seek to file table
    // 定位到文件表
    input.seekg(static_cast<std::streamoff>(table_offset));
    if (!input) {
        if (verbose)
            std::cout << "错误: 定位到文件表开始位置失败" << std::endl;
        input.close();
        return Result::ERROR_READ_FILE_TABLE;
    }

    // Read file table
    // 读取文件表
    file_table_.clear();
    file_index_.clear();
    for (uint32_t i = 0; i < file_count; ++i) {
        if (!input)
            break;

        FileEntry entry;
        uint16_t name_length;
        input.read(reinterpret_cast<char*>(&name_length), sizeof(name_length));
        if (!input || name_length == 0 || name_length > 4096) {
            if (verbose)
                std::cout << "错误: 无效的文件名长度: " << name_length << std::endl;
            return Result::ERROR_CORRUPTED_PACKAGE;
        }

        if (name_length > 0) {
            std::vector<char> name_buffer(name_length);
            input.read(name_buffer.data(), name_length);
            if (input) {
                entry.filename = std::string(name_buffer.data(), name_length);
                if (!utf8::is_valid(entry.filename.begin(), entry.filename.end())) {
                    entry.filename = "file_" + std::to_string(i);
                }
            }
        }

        const fs::path internal_path(entry.filename);
        if (internal_path.is_absolute() || internal_path.has_root_name()) {
            return Result::ERROR_CORRUPTED_PACKAGE;
        }
        for (const auto& part : internal_path) {
            if (part == "..")
                return Result::ERROR_CORRUPTED_PACKAGE;
        }

        // Read offset and size
        // 读取偏移量和大小
        uint64_t offset, size;
        input.read(reinterpret_cast<char*>(&offset), sizeof(offset));
        input.read(reinterpret_cast<char*>(&size), sizeof(size));
        if (!input)
            return Result::ERROR_CORRUPTED_PACKAGE;

        if (offset < 10 || offset > table_offset || size > table_offset - offset) {
            if (verbose) {
                std::cout << "警告: 跳过无效的文件条目: " << entry.filename << " (offset=" << offset
                          << ", size=" << size << ")" << std::endl;
            }
            return Result::ERROR_CORRUPTED_PACKAGE;
        }

        entry.offset = offset;
        entry.size = size;

        // Read file type
        // 读取文件类型
        uint8_t type_length;
        input.read(reinterpret_cast<char*>(&type_length), sizeof(type_length));
        if (!input)
            return Result::ERROR_CORRUPTED_PACKAGE;

        if (type_length > 0) {
            std::vector<char> type_buffer(type_length);
            input.read(type_buffer.data(), type_length);
            if (input) {
                entry.type = std::string(type_buffer.data(), type_length);
                if (!utf8::is_valid(entry.type.begin(), entry.type.end())) {
                    entry.type = ".dat";
                }
            } else
                return Result::ERROR_CORRUPTED_PACKAGE;
        }

        if (file_index_.contains(entry.filename))
            return Result::ERROR_CORRUPTED_PACKAGE;

        file_table_.push_back(entry);
        file_index_[entry.filename] = file_table_.size() - 1;
    }

    input.close();

    if (file_table_.empty()) {
        return Result::ERROR_READ_FILE_TABLE;
    }

    if (verbose) {
        std::cout << "成功加载 " << file_table_.size() << " 个文件条目" << std::endl;
    }

    return Result::SUCCESS;
}

// ==================== In-memory unpacking functionality implementation ====================
// ==================== 内存解包功能实现 ====================
auto Unpackager::ExtractFileToMemory(const std::string& filename, MemoryFile& memoryFile) -> Result {
    const auto it = file_index_.find(filename);
    if (it == file_index_.end()) {
        return Result::ERROR_FILES_NOT_FOUND;
    }

    const FileEntry& entry = file_table_[it->second];
    std::vector<char> buffer;
    const Result result = ReadFileData(entry, buffer);
    if (result != Result::SUCCESS) {
        return result;
    }

    memoryFile.filename = entry.filename;
    memoryFile.type = entry.type;
    memoryFile.data = std::move(buffer);

    return Result::SUCCESS;
}

auto Unpackager::ExtractFileToMemory(const std::string& filename) -> std::unique_ptr<MemoryFile> {
    auto memoryFile = std::make_unique<MemoryFile>();
    const Result result = ExtractFileToMemory(filename, *memoryFile);
    if (result == Result::SUCCESS) {
        return memoryFile;
    }
    return nullptr;
}

auto Unpackager::ExtractFilesToMemory(const std::vector<std::string>& filenames, std::vector<MemoryFile>& memoryFiles)
    -> Result {
    memoryFiles.clear();

    for (const auto& filename : filenames) {
        MemoryFile memoryFile;
        const Result result = ExtractFileToMemory(filename, memoryFile);
        if (result == Result::SUCCESS) {
            memoryFiles.push_back(std::move(memoryFile));
        } else {
            std::cout << "警告: 无法提取文件到内存: " << filename << std::endl;
        }
    }

    return memoryFiles.empty() ? Result::ERROR_FILES_NOT_FOUND : Result::SUCCESS;
}

auto Unpackager::ExtractAllToMemory(std::vector<MemoryFile>& memoryFiles) -> Result {
    memoryFiles.clear();

    for (const auto& entry : file_table_) {
        MemoryFile memoryFile;
        const Result result = ExtractFileToMemory(entry.filename, memoryFile);
        if (result == Result::SUCCESS) {
            memoryFiles.push_back(std::move(memoryFile));
        } else {
            std::cout << "警告: 无法提取文件到内存: " << entry.filename << std::endl;
        }
    }

    return memoryFiles.empty() ? Result::ERROR_READ_DATA : Result::SUCCESS;
}

auto Unpackager::GetFileData(const std::string& filename, const char** data, size_t* size) -> Result {
    if (data == nullptr || size == nullptr) {
        return Result::ERROR_READ_DATA;
    }
    const auto it = file_index_.find(filename);
    if (it == file_index_.end()) {
        return Result::ERROR_FILES_NOT_FOUND;
    }

    const FileEntry& entry = file_table_[it->second];
    const Result result = ReadFileData(entry, last_read_buffer_);
    if (result != Result::SUCCESS) {
        return result;
    }

    *data = last_read_buffer_.data();
    *size = last_read_buffer_.size();

    return Result::SUCCESS;
}

// ==================== Disk unpacking functionality implementation ====================
// ==================== 磁盘解包功能实现 ====================
auto Unpackager::UnpackAll(const Config& config) -> Result {
    if (file_table_.empty()) {
        return Result::ERROR_READ_FILE_TABLE;
    }

    auto overall_result = Result::SUCCESS;
    int success_count = 0;

    for (const auto& entry : file_table_) {
        const Result file_result = ExtractFile(entry.filename, config);
        if (file_result == Result::SUCCESS) {
            success_count++;
        } else if (overall_result == Result::SUCCESS) {
            overall_result = file_result;
        }
    }

    if (config.verbose) {
        std::cout << "解包完成: " << success_count << "/" << file_table_.size() << " 个文件解压到 " << config.outputDir
                  << std::endl;
    }

    return overall_result;
}

auto Unpackager::ExtractFile(const std::string& filename, const Config& config) -> Result {
    auto it = file_index_.find(filename);
    if (it == file_index_.end()) {
        if (config.verbose) {
            std::cout << "错误: 文件未找到 - " << filename << std::endl;
        }
        return Result::ERROR_EXTRACT_FILE;
    }

    const FileEntry& entry = file_table_[it->second];

    // Build output path
    // 构建输出路径
    fs::path output_path;
    try {
        output_path = config.outputDir;
        if (config.preserveStructure) {
            output_path /= filename;
        } else {
            fs::path file_path(filename);
            output_path /= file_path.filename();
        }
    } catch (const std::exception& e) {
        if (config.verbose) {
            std::cout << "错误: 构建输出路径失败: " << e.what() << std::endl;
        }
        return Result::ERROR_ENCODING;
    }

    // Create directory
    // 创建目录
    fs::path output_dir = output_path.parent_path();
    if (!CreateDirectory(output_dir)) {
        return Result::ERROR_CREATE_DIRECTORY;
    }

    // Check if file already exists
    // 检查文件是否已存在
    try {
        if (fs::exists(output_path) && !config.overwrite) {
            if (config.verbose) {
                std::cout << "跳过: 文件已存在 - " << SafePathToString(output_path) << std::endl;
            }
            return Result::SUCCESS;
        }
    } catch (const std::exception& e) {
        if (config.verbose) {
            std::cout << "警告: 检查文件存在性失败: " << e.what() << std::endl;
        }
    }

    // Read file data
    // 读取文件数据
    std::vector<char> buffer;
    Result read_result = ReadFileData(entry, buffer);
    if (read_result != Result::SUCCESS) {
        return read_result;
    }

    // Write file
    // 写入文件
    std::ofstream output;
    try {
        output.open(output_path, std::ios::binary);
    } catch (const std::exception& e) {
        if (config.verbose) {
            std::cout << "错误: 无法创建输出文件 " << SafePathToString(output_path) << ": " << e.what() << std::endl;
        }
        return Result::ERROR_EXTRACT_FILE;
    }

    if (!output.is_open()) {
        return Result::ERROR_EXTRACT_FILE;
    }

    output.write(buffer.data(), static_cast<long long>(entry.size));
    output.close();

    if (config.verbose) {
        std::cout << "解压: " << SafePathToString(output_path) << " (" << entry.size << " 字节)" << std::endl;
    }

    return Result::SUCCESS;
}

// ==================== Query functionality implementation ====================
// ==================== 查询功能实现 ====================
auto Unpackager::GetFileList() const -> std::vector<std::string> {
    std::vector<std::string> files;
    for (const auto& entry : file_table_) {
        files.push_back(entry.filename);
    }
    return files;
}

auto Unpackager::Contains(const std::string& filename) const -> bool {
    return file_index_.contains(filename);
}

auto Unpackager::GetFileInfo(const std::string& filename) const -> const FileEntry* {
    const auto it = file_index_.find(filename);
    if (it != file_index_.end()) {
        return &file_table_[it->second];
    }
    return nullptr;
}

auto Unpackager::PrintPackageInfo() const -> void {
    std::cout << "包文件: " << package_path_ << std::endl;
    std::cout << "包含 " << file_table_.size() << " 个文件:" << std::endl;

    uint64_t total_size = 0;
    for (const auto& entry : file_table_) {
        // Entry names are stored as UTF-8; print them as-is (the console is
        // expected to run with a UTF-8 code page). Only bound the length for
        // readability.
        std::string display_name = entry.filename;
        if (display_name.length() > 50) {
            display_name = display_name.substr(0, 47) + "...";
        }

        std::cout << "  " << display_name << " [" << entry.type << "] - " << entry.size << " 字节" << std::endl;
        total_size += entry.size;
    }

    std::cout << "总大小: " << total_size << " 字节" << std::endl;
}
} // namespace atom::tools
