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
    // Stream object movement is complex; the new object reopens as needed.
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
        std::cout << "Error: failed to create directory " << dir_path << ": " << e.what() << std::endl;
        return false;
    }
}

auto Unpackager::SafePathToString(const fs::path& path) -> std::string {
    try {
        return path.string();
    } catch (const std::exception& e) {
        std::cout << "Warning: path conversion error: " << e.what() << std::endl;
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
            std::cout << "Error: package file does not exist: " << packageFile << std::endl;
        return Result::ERROR_OPEN_FILE;
    }

    uint64_t package_size = GetFileSize(packageFile);
    if (verbose)
        std::cout << "Package file size: " << package_size << " bytes" << std::endl;

    if (package_size < 20) {
        if (verbose)
            std::cout << "Error: file is too small to be a valid package" << std::endl;
        return Result::ERROR_INVALID_FORMAT;
    }

    std::ifstream input(packageFile, std::ios::binary);
    if (!input.is_open()) {
        if (verbose)
            std::cout << "Error: cannot open package file" << std::endl;
        return Result::ERROR_OPEN_FILE;
    }

    // Read magic number
    char magic[4];
    input.read(magic, 4);
    if (!input) {
        if (verbose)
            std::cout << "Error: failed to read file header" << std::endl;
        input.close();
        return Result::ERROR_INVALID_FORMAT;
    }

    std::string magic_str(magic, 4);
    if (verbose)
        std::cout << "File magic: " << magic_str << std::endl;

    if (!std::equal(std::begin(MAGIC), std::end(MAGIC), magic)) {
        if (verbose)
            std::cout << "Error: invalid magic number, expected 'APKG'" << std::endl;
        input.close();
        return Result::ERROR_INVALID_FORMAT;
    }

    // Read version number
    uint16_t version;
    input.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (!input) {
        if (verbose)
            std::cout << "Error: failed to read version number" << std::endl;
        input.close();
        return Result::ERROR_INVALID_FORMAT;
    }

    if (verbose)
        std::cout << "File version: " << version << std::endl;
    if (version != VERSION) {
        if (verbose)
            std::cout << "Error: unsupported version number: " << version << std::endl;
        input.close();
        return Result::ERROR_INVALID_FORMAT;
    }

    // Read file count
    uint32_t file_count;
    input.read(reinterpret_cast<char*>(&file_count), sizeof(file_count));
    if (!input) {
        if (verbose)
            std::cout << "Error: failed to read file count" << std::endl;
        input.close();
        return Result::ERROR_INVALID_FORMAT;
    }

    if (verbose)
        std::cout << "File count: " << file_count << std::endl;
    if (file_count > 1000000) {
        if (verbose)
            std::cout << "Error: abnormal file count: " << file_count << std::endl;
        input.close();
        return Result::ERROR_CORRUPTED_PACKAGE;
    }

    // Read file table offset
    input.seekg(-static_cast<std::streamoff>(sizeof(uint64_t)), std::ios::end);
    if (!input) {
        if (verbose)
            std::cout << "Error: failed to seek to file table offset" << std::endl;
        input.close();
        return Result::ERROR_INVALID_FORMAT;
    }

    uint64_t table_offset;
    input.read(reinterpret_cast<char*>(&table_offset), sizeof(table_offset));
    if (!input) {
        if (verbose)
            std::cout << "Error: failed to read file table offset" << std::endl;
        input.close();
        return Result::ERROR_INVALID_FORMAT;
    }

    if (verbose)
        std::cout << "File table offset: " << table_offset << std::endl;
    if (table_offset >= package_size || table_offset < 10) {
        if (verbose)
            std::cout << "Error: invalid file table offset: " << table_offset << std::endl;
        input.close();
        return Result::ERROR_CORRUPTED_PACKAGE;
    }

    // Seek to file table
    input.seekg(static_cast<std::streamoff>(table_offset));
    if (!input) {
        if (verbose)
            std::cout << "Error: failed to seek to file table start" << std::endl;
        input.close();
        return Result::ERROR_READ_FILE_TABLE;
    }

    // Read file table
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
                std::cout << "Error: invalid filename length: " << name_length << std::endl;
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
        uint64_t offset, size;
        input.read(reinterpret_cast<char*>(&offset), sizeof(offset));
        input.read(reinterpret_cast<char*>(&size), sizeof(size));
        if (!input)
            return Result::ERROR_CORRUPTED_PACKAGE;

        if (offset < 10 || offset > table_offset || size > table_offset - offset) {
            if (verbose) {
                std::cout << "Warning: skipping invalid file entry: " << entry.filename << " (offset=" << offset
                          << ", size=" << size << ")" << std::endl;
            }
            return Result::ERROR_CORRUPTED_PACKAGE;
        }

        entry.offset = offset;
        entry.size = size;

        // Read file type
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
        std::cout << "Successfully loaded " << file_table_.size() << " file entries" << std::endl;
    }

    return Result::SUCCESS;
}

// ==================== In-memory unpacking functionality ====================
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
            std::cout << "Warning: failed to extract file to memory: " << filename << std::endl;
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
            std::cout << "Warning: failed to extract file to memory: " << entry.filename << std::endl;
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

// ==================== Disk unpacking functionality ====================
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
        std::cout << "Extraction complete: " << success_count << "/" << file_table_.size() << " files extracted to "
                  << config.outputDir << std::endl;
    }

    return overall_result;
}

auto Unpackager::ExtractFile(const std::string& filename, const Config& config) -> Result {
    auto it = file_index_.find(filename);
    if (it == file_index_.end()) {
        if (config.verbose) {
            std::cout << "Error: file not found - " << filename << std::endl;
        }
        return Result::ERROR_EXTRACT_FILE;
    }

    const FileEntry& entry = file_table_[it->second];

    // Build output path
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
            std::cout << "Error: failed to build output path: " << e.what() << std::endl;
        }
        return Result::ERROR_ENCODING;
    }

    // Create directory
    fs::path output_dir = output_path.parent_path();
    if (!CreateDirectory(output_dir)) {
        return Result::ERROR_CREATE_DIRECTORY;
    }

    // Check if the file already exists
    try {
        if (fs::exists(output_path) && !config.overwrite) {
            if (config.verbose) {
                std::cout << "Skipped: file already exists - " << SafePathToString(output_path) << std::endl;
            }
            return Result::SUCCESS;
        }
    } catch (const std::exception& e) {
        if (config.verbose) {
            std::cout << "Warning: failed to check file existence: " << e.what() << std::endl;
        }
    }

    // Read file data
    std::vector<char> buffer;
    Result read_result = ReadFileData(entry, buffer);
    if (read_result != Result::SUCCESS) {
        return read_result;
    }

    // Write file
    std::ofstream output;
    try {
        output.open(output_path, std::ios::binary);
    } catch (const std::exception& e) {
        if (config.verbose) {
            std::cout << "Error: cannot create output file " << SafePathToString(output_path) << ": " << e.what()
                      << std::endl;
        }
        return Result::ERROR_EXTRACT_FILE;
    }

    if (!output.is_open()) {
        return Result::ERROR_EXTRACT_FILE;
    }

    output.write(buffer.data(), static_cast<long long>(entry.size));
    output.close();

    if (config.verbose) {
        std::cout << "Extracted: " << SafePathToString(output_path) << " (" << entry.size << " bytes)" << std::endl;
    }

    return Result::SUCCESS;
}

// ==================== Query functionality ====================
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
    std::cout << "Package file: " << package_path_ << std::endl;
    std::cout << "Contains " << file_table_.size() << " files:" << std::endl;

    uint64_t total_size = 0;
    for (const auto& entry : file_table_) {
        // Entry names are stored as UTF-8; print them as-is (the console is
        // expected to run with a UTF-8 code page). Only bound the length for
        // readability.
        std::string display_name = entry.filename;
        if (display_name.length() > 50) {
            display_name = display_name.substr(0, 47) + "...";
        }

        std::cout << "  " << display_name << " [" << entry.type << "] - " << entry.size << " bytes" << std::endl;
        total_size += entry.size;
    }

    std::cout << "Total size: " << total_size << " bytes" << std::endl;
}
} // namespace atom::tools
