// Self Dependency
#include "Packager.hpp"

// Standard Library
#include <algorithm>
#include <iostream>

// Third party Library
#include <utf8.h>

// Engine Headers
#include <Log/LogSystem.hpp>

namespace atom::tools {
Packager::Config::Config() : compress(false), verbose(false), preserveStructure(true), overwrite(true) {}

auto Packager::NormalizePath(const std::string& path) -> std::string {
    try {
        std::string result = path;
        std::ranges::replace(result, '\\', '/');
        if (result.size() >= 2 && result[0] == '.' && result[1] == '/') {
            result = result.substr(2);
        }
        return result;
    } catch (...) {
        return path;
    }
}

auto Packager::IsValidUTF8(const std::string& str) -> bool {
    try {
        return utf8::is_valid(str.begin(), str.end());
    } catch (...) {
        return false;
    }
}

auto Packager::ToUTF8(const std::string& str) -> std::string {
    try {
        if (!IsValidUTF8(str)) {
            std::string temp;
            utf8::replace_invalid(str.begin(), str.end(), std::back_inserter(temp));
            return temp;
        }
        return str;
    } catch (...) {
        return "invalid_encoding_file";
    }
}

auto Packager::SafePathToString(const fs::path& path) -> std::string {
    try {
        return path.string();
    } catch (const std::exception& e) {
        LOG_ERROR(atom::utilities::LogChannel::PACKAGER, "Path conversion error: " + std::to_string(*e.what()));
        return "unknown_path";
    }
}

auto Packager::SafeRelativePath(const fs::path& path) -> std::string {
    try {
        return fs::relative(path).string();
    } catch (const std::exception& e) {
        LOG_ERROR(atom::utilities::LogChannel::PACKAGER,
                  "Unable to obtain relative path: " + std::to_string(*e.what()));
        return path.filename().string();
    }
}

auto Packager::CollectFiles(const std::vector<std::string>& resourcePaths, std::vector<fs::path>& allFiles,
                            const Config& config) -> bool {
    for (const auto& path_str : resourcePaths) {
        try {
            fs::path path(path_str);
            if (!fs::exists(path)) {
                LOG_WARNING(atom::utilities::LogChannel::PACKAGER, "Path does not exist: " + path_str);
                continue;
            }

            if (fs::is_directory(path)) {
                for (const auto& entry : fs::recursive_directory_iterator(path)) {
                    if (entry.is_regular_file()) {
                        allFiles.push_back(entry.path());
                    }
                }
            } else if (fs::is_regular_file(path)) {
                allFiles.push_back(path);
            }
        } catch (const std::exception& e) {
            LOG_WARNING(atom::utilities::LogChannel::PACKAGER,
                        "Error occurred while processing the path " + path_str + ": " + e.what());
        }
    }
    return !allFiles.empty();
}

auto Packager::GenerateInternalFilename(const fs::path& filePath, const Config& config) -> std::string {
    try {
        if (config.preserveStructure) {
            const std::string relative_path = SafeRelativePath(filePath);
            // fs::relative() yields an empty path when the two paths sit on
            // different drives (libstdc++) or throws (MSVC); fall back to the
            // bare filename so entries never get an empty name.
            if (!relative_path.empty())
                return ToUTF8(NormalizePath(relative_path));
            return ToUTF8(NormalizePath(SafePathToString(filePath.filename())));
        } else {
            const std::string filename = ToUTF8(SafePathToString(filePath.filename()));
            int counter = 1;
            std::string final_name = filename;

            while (file_index_.contains(final_name)) {
                std::string stem = ToUTF8(SafePathToString(filePath.stem()));
                const std::string extension = ToUTF8(SafePathToString(filePath.extension()));
                final_name = stem + "_" + std::to_string(counter) += extension;
                counter++;
            }
            return final_name;
        }
    } catch (const std::exception& e) {
        LOG_ERROR(atom::utilities::LogChannel::PACKAGER,
                  "Failed to generate internal filename: " + std::to_string(*e.what()));
        return "unknown_file_" + std::to_string(file_index_.size());
    }
}

auto Packager::Pack(const std::vector<std::string>& resourcePaths, const std::string& outputFile, const Config& config)
    -> Result {
    file_table_.clear();
    file_index_.clear();

    try {
        if (fs::exists(outputFile)) {
            if (!config.overwrite) {
                return Result::ERROR_WRITE_FAILED;
            }
            fs::remove(outputFile);
        }
    } catch (const std::exception& e) {
        LOG_ERROR(atom::utilities::LogChannel::PACKAGER,
                  "Unable to delete existing file: " + std::to_string(*e.what()));
        return Result::ERROR_WRITE_FAILED;
    }

    std::vector<fs::path> allFiles;
    if (!CollectFiles(resourcePaths, allFiles, config)) {
        return Result::ERROR_EMPTY_PACKAGE;
    }

    std::ofstream output(outputFile, std::ios::binary);
    if (!output.is_open()) {
        return Result::ERROR_OPEN_OUTPUT;
    }

    // Write file header
    output.write(MAGIC, 4);
    output.write(reinterpret_cast<const char*>(&VERSION), sizeof(VERSION));

    // Reserve file table position - write file count as 0 initially, update later
    uint64_t file_table_offset = output.tellp();
    uint32_t file_count = 0;
    output.write(reinterpret_cast<const char*>(&file_count), sizeof(file_count));

    // Write file data
    uint64_t current_offset = file_table_offset + sizeof(file_count);
    std::vector<FileEntry> entries;

    for (const auto& filePath : allFiles) {
        std::ifstream input;
        try {
            input.open(filePath, std::ios::binary);
        } catch (const std::exception& e) {
            LOG_WARNING(atom::utilities::LogChannel::PACKAGER,
                        "Warning: Unable to open file: " + SafePathToString(filePath) + " - " + e.what());
            continue;
        }

        if (!input.is_open()) {
            LOG_WARNING(atom::utilities::LogChannel::PACKAGER,
                        "Unable to open file: " + SafePathToString(filePath));
            continue;
        }

        // Get file size
        input.seekg(0, std::ios::end);
        uint64_t file_size = static_cast<uint64_t>(input.tellg());
        input.seekg(0, std::ios::beg);

        // Create file entry
        FileEntry entry;
        try {
            entry.original_path = SafePathToString(filePath);
            entry.filename = GenerateInternalFilename(filePath, config);
            entry.type = ToUTF8(SafePathToString(filePath.extension()));
            entry.offset = current_offset;
            entry.size = file_size;
        } catch (const std::exception& e) {
            LOG_ERROR(atom::utilities::LogChannel::PACKAGER,
                      "Error: Failed to create file entry: " + std::to_string(*e.what()));
            input.close();
            continue;
        }

        // Read and write file data
        std::vector<char> buffer(file_size);
        input.read(buffer.data(), static_cast<long long>(file_size));

        if (!input) {
            LOG_WARNING(atom::utilities::LogChannel::PACKAGER,
                        "Failed to read file: " + SafePathToString(filePath));
            input.close();
            continue;
        }
        input.close();

        output.write(buffer.data(), static_cast<long long>(file_size));
        if (!output) {
            return Result::ERROR_WRITE_FAILED;
        }

        current_offset += file_size;
        entries.push_back(entry);
        file_count++;

        LOG_INFO(atom::utilities::LogChannel::PACKAGER,
                 "Packing: " + entry.filename + " (" + std::to_string(file_size) + " bytes)");
    }

    if (file_count == 0) {
        output.close();
        try {
            fs::remove(outputFile);
        } catch (...) {
        }
        return Result::ERROR_EMPTY_PACKAGE;
    }

    // Record file table start position
    uint64_t table_start = output.tellp();

    // Update file count
    output.seekp(static_cast<std::streampos>(static_cast<long long>(file_table_offset)));
    output.write(reinterpret_cast<const char*>(&file_count), sizeof(file_count));
    output.seekp(static_cast<std::streampos>(static_cast<long long>(table_start)));

    // Write each file entry
    for (const auto& entry : entries) {
        // Write filename
        std::string safe_filename = ToUTF8(entry.filename);
        if (safe_filename.size() > UINT16_MAX) {
            return Result::ERROR_INVALID_PATH;
        }
        auto name_length = static_cast<uint16_t>(safe_filename.length());
        output.write(reinterpret_cast<const char*>(&name_length), sizeof(name_length));
        output.write(safe_filename.c_str(), name_length);

        // Write file info
        uint64_t offset = entry.offset;
        uint64_t size = entry.size;
        output.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
        output.write(reinterpret_cast<const char*>(&size), sizeof(size));

        // Write file type
        std::string safe_type = ToUTF8(entry.type);
        if (safe_type.size() > UINT8_MAX) {
            return Result::ERROR_INVALID_PATH;
        }
        auto type_length = static_cast<uint8_t>(safe_type.length());
        output.write(reinterpret_cast<const char*>(&type_length), sizeof(type_length));
        output.write(safe_type.c_str(), type_length);

        // Save to internal file table
        file_table_.push_back(entry);
        file_index_[entry.filename] = file_table_.size() - 1;
    }

    // Write file table offset
    uint64_t table_end = static_cast<uint64_t>(output.tellp());
    output.write(reinterpret_cast<const char*>(&table_start), sizeof(table_start));

    output.close();

    // Verify package file
    LOG_INFO(atom::utilities::LogChannel::PACKAGER, "Packing complete, commencing verification...");
    std::ifstream verify(outputFile, std::ios::binary);
    if (verify.is_open()) {
        char magic[4];
        verify.read(magic, 4);
        std::string magic_str(magic, 4);
        LOG_INFO(atom::utilities::LogChannel::PACKAGER,
                 "Verification magic number: " + magic_str + " " +
                     (magic_str == std::string(MAGIC, 4) ? "True" : "False"));

        uint16_t version;
        verify.read(reinterpret_cast<char*>(&version), sizeof(version));
        LOG_INFO(atom::utilities::LogChannel::PACKAGER,
                 "Verification version: " + std::to_string(version) + " " + (version == 1 ? "True" : "False"));

        uint32_t file_count_verify;
        verify.read(reinterpret_cast<char*>(&file_count_verify), sizeof(file_count_verify));
        LOG_INFO(atom::utilities::LogChannel::PACKAGER,
                 "Number of documents to be verified: " + std::to_string(file_count_verify) + " " +
                     (file_count_verify == file_count ? "True" : "False"));

        // Read file table offset
        verify.seekg(-static_cast<std::streamoff>(sizeof(uint64_t)), std::ios::end);
        uint64_t table_offset_verify;
        verify.read(reinterpret_cast<char*>(&table_offset_verify), sizeof(table_offset_verify));
        LOG_INFO(atom::utilities::LogChannel::PACKAGER,
                 "Verification file table offset: " + std::to_string(table_offset_verify) + " " +
                     (table_offset_verify == table_start ? "True" : "False"));

        verify.close();
    }

    LOG_INFO(atom::utilities::LogChannel::PACKAGER, "Packing completed: " + outputFile);
    LOG_INFO(atom::utilities::LogChannel::PACKAGER, "Includes " + std::to_string(file_count) + " files");
    LOG_INFO(atom::utilities::LogChannel::PACKAGER,
             "Package size: " + std::to_string(table_end + sizeof(uint64_t)) + " bytes");

    return Result::SUCCESS;
}

auto Packager::GetPackedFiles() const -> std::vector<std::string> {
    std::vector<std::string> files;
    for (const auto& entry : file_table_) {
        files.push_back(entry.filename);
    }
    return files;
}

auto Packager::PrintPackageInfo() const -> void {
    LOG_INFO(atom::utilities::LogChannel::PACKAGER,
             "The package contains " + std::to_string(file_table_.size()) + " files:");
    for (const auto& entry : file_table_) {
        LOG_INFO(atom::utilities::LogChannel::PACKAGER,
                 "  " + entry.filename + " [" + entry.type + "] - " + std::to_string(entry.size) + " bytes");
    }
}
} // namespace atom::tools
