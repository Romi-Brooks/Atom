// Standard Library
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <limits>
#include <sstream>
#include <algorithm>

#ifdef _WIN32
#include "windows.h"
#endif // _WIN32

// Engine Headers
#include <Log/LogSystem.hpp>

// Self Dependencies
#include "Packager.hpp"
#include "Unpackager.hpp"

namespace fs = std::filesystem;
using atom::tools::Packager;
using atom::tools::Unpackager;

// Clear input buffer
auto ClearInputBuffer() -> void {
    std::cin.clear(); // Clear error state
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

// Recursively traverse a single directory, collecting all file paths
auto TraverseSingleDirectory(const std::string& dir_path) -> std::vector<std::string> {
    std::vector<std::string> file_paths;
    try {
        if (!fs::exists(dir_path)) {
            LOG_ERROR(atom::LogChannel::ATOM_UTILITIES_PACKAGER, "Directory does not exist:" + dir_path);
            return file_paths;
        }
        if (!fs::is_directory(dir_path)) {
            LOG_ERROR(atom::LogChannel::ATOM_UTILITIES_PACKAGER, "Not a valid directory:" + dir_path);
            return file_paths;
        }

        // Recursively traverse directories, collecting only regular files
        for (const auto& entry : fs::recursive_directory_iterator(dir_path)) {
            if (fs::is_regular_file(entry)) {
                file_paths.push_back(entry.path().string());
            }
        }
    } catch (const fs::filesystem_error& e) {
        LOG_ERROR(atom::LogChannel::ATOM_UTILITIES_PACKAGER,
                  "Directory traversal failed:" + dir_path + " -> " + e.what());
    }
    return file_paths;
}

// Traverse multiple directories, collecting all file paths
auto TraverseMultipleDirectories(const std::vector<std::string>& dir_paths) -> std::vector<std::string> {
    std::vector<std::string> all_file_paths;
    for (const auto& dir : dir_paths) {
        auto single_dir_files = TraverseSingleDirectory(dir);
        // Merge file paths from a single directory into the total list
        all_file_paths.insert(all_file_paths.end(), single_dir_files.begin(), single_dir_files.end());
    }
    return all_file_paths;
}

// Parse user input for multiple directories
auto ParseMultiDirectories(const std::string& input) -> std::vector<std::string> {
    std::vector<std::string> dir_paths;
    std::stringstream ss(input);
    std::string dir;

    // Split the input string by comma
    while (std::getline(ss, dir, ',')) {
        // Remove leading/trailing spaces from directory names: res1/, res2/
        dir.erase(dir.begin(), std::ranges::find_if(dir, [](const unsigned char c) { return !std::isspace(c); }));
        dir.erase(std::find_if(dir.rbegin(), dir.rend(), [](const unsigned char c) { return !std::isspace(c); }).base(),
                  dir.end());

        if (!dir.empty()) {
            dir_paths.push_back(dir);
        }
    }
    return dir_paths;
}

// Packing function
auto PackFiles(const std::string& packName, const std::vector<std::string>& resourcePath) -> bool {
    Packager packer;
    Packager::Config config;
    config.verbose = true;

    LOG_INFO(atom::LogChannel::ATOM_UTILITIES_PACKAGER,
             "Commencing packing... Total number of files awaiting packing:" + std::to_string(resourcePath.size()));
    const auto result = packer.Pack(resourcePath, packName, config);

    if (result == Packager::Result::SUCCESS) {
        LOG_INFO(atom::LogChannel::ATOM_UTILITIES_PACKAGER, "Packing successful!");
        packer.PrintPackageInfo();
        return true;
    } else {
        LOG_ERROR(atom::LogChannel::ATOM_UTILITIES_PACKAGER, "Packing failed!");
        return false;
    }
}

// Original unpacking function
auto UnpackAllToFolder(const std::string& packName) -> bool {
    Unpackager unpacker;
    if (unpacker.Load(packName) == Unpackager::Result::SUCCESS) {
        unpacker.PrintPackageInfo();
        // Unpack to disk
        Unpackager::Config unpackConfig;
        unpackConfig.verbose = true;
        unpackConfig.outputDir = "extract/";
        unpackConfig.preserveStructure = true;
        const auto result = unpacker.UnpackAll(unpackConfig);
        if (result == Unpackager::Result::SUCCESS) {
            LOG_INFO(atom::LogChannel::ATOM_UTILITIES_PACKAGER, "Unpacking successful!");
            return true;
        }
        LOG_ERROR(atom::LogChannel::ATOM_UTILITIES_PACKAGER, "Unpacking failed while extracting files!");
        return false;
    } else {
        LOG_ERROR(atom::LogChannel::ATOM_UTILITIES_PACKAGER, "Unpacking failed!");
        return false;
    }
}

auto main() -> int {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif // _WIN32

    std::cout << "================================================" << std::endl;
    std::cout << "Atom Resource Package / Unpackage Tools v1.0  " << std::endl;
    std::cout << "================================================" << std::endl;

    int choice = 0;
    while (true) {
        // Display menu
        std::cout << "\nOperations: " << std::endl;
        std::cout << "1. Pack from multiple target folders" << std::endl;
        std::cout << "2. Extract to the extract folder" << std::endl;
        std::cout << "0. Exit" << std::endl;
        std::cout << "Input options (0-2):";

        // Handle user input
        if (!(std::cin >> choice)) {
            ClearInputBuffer();
            LOG_ERROR(atom::LogChannel::ATOM_UTILITIES_PACKAGER, "Please enter a valid number (0-2)!");
            continue;
        }
        ClearInputBuffer(); // Clear buffer to avoid affecting subsequent string input

        // Execute function based on selection
        switch (choice) {
        case 1: {
            std::string dir_input, pack_name;
            std::cout << "\nTips: Enter multiple directories separated by commas (e.g.:resources/,audio/,textures/):"
                      << std::endl;
            std::cout << "Please enter the directory paths to be packed:";
            std::getline(std::cin, dir_input);
            std::cout << "Please enter the output package name (e.g. media_res.dat):";
            std::getline(std::cin, pack_name);

            // 1. Parse user input for multiple directories
            std::vector<std::string> dir_paths = ParseMultiDirectories(dir_input);
            if (dir_paths.empty()) {
                LOG_ERROR(atom::LogChannel::ATOM_UTILITIES_PACKAGER, "Error: No valid directories entered!");
                break;
            }
            LOG_INFO(atom::LogChannel::ATOM_UTILITIES_PACKAGER,
                     "Successfully parsed " + std::to_string(dir_paths.size()) + " directories to pack.");

            // 2. Traverse multiple directories, collecting all file paths
            std::vector<std::string> all_files = TraverseMultipleDirectories(dir_paths);
            if (all_files.empty()) {
                LOG_ERROR(atom::LogChannel::ATOM_UTILITIES_PACKAGER,
                          "Error: No files found in the entered directories!");
                break;
            }

            // 3. Call the packing function
            PackFiles(pack_name, all_files);
            break;
        }
        case 2: {
            std::string pack_name;
            LOG_INFO(atom::LogChannel::ATOM_UTILITIES_PACKAGER,
                     "\nPlease enter the name of the package file to be unpacked (e.g., media_res.dat):");
            std::getline(std::cin, pack_name);
            UnpackAllToFolder(pack_name);
            break;
        }
        case 0: {
            LOG_INFO(atom::LogChannel::ATOM_UTILITIES_PACKAGER, "\nExited. Goodbye!");
            return 0;
        }
        default:
            LOG_ERROR(atom::LogChannel::ATOM_UTILITIES_PACKAGER,
                      "Invalid option, please enter a number between 0 and 2!");
            break;
        }

        std::cout << "\n-------------------------------------" << std::endl;
        std::cout << "Press Enter to continue...";
        std::cin.get();
    }
}
