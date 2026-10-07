// Standard Library
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <limits>
#include <sstream>
#include <algorithm>

// Engine Headers
#include <Log/LogSystem.hpp>
#include <Utilities/Utf8/Utf8.hpp>

// Self Dependencies
#include "Packager.hpp"
#include "PackageBuilder.hpp"
#include "Unpackager.hpp"

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#endif

namespace native_fs = std::filesystem;
using atom::tools::Packager;
using atom::tools::PackageBuilder;
using atom::tools::Unpackager;

// ---------------------------------------------------------------------------
// UTF-8 command-line arguments
//
// On Windows, the CRT converts the process command line to narrow argv using
// the ANSI code page (GBK on Chinese systems), which corrupts UTF-8 paths.
// We re-parse the wide command line directly and convert to UTF-8 ourselves.
// ---------------------------------------------------------------------------
static auto CollectUtf8Args(int argc, char** argv) -> std::vector<std::string> {
#if defined(_WIN32)
    (void)argc;
    (void)argv;
    int wide_argc = 0;
    LPWSTR* wide_argv = CommandLineToArgvW(GetCommandLineW(), &wide_argc);
    if (wide_argv == nullptr)
        return {};
    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(wide_argc));
    for (int i = 0; i < wide_argc; ++i)
        args.push_back(atom::Utf8FromWide(wide_argv[i]));
    LocalFree(wide_argv);
    return args;
#else
    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(argc));
    for (int i = 0; i < argc; ++i)
        args.emplace_back(argv[i]);
    return args;
#endif
}

// Clear input buffer
static auto ClearInputBuffer() -> void {
    std::cin.clear(); // Clear error state
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

// Recursively traverse a single directory, collecting all file paths
static auto TraverseSingleDirectory(const std::string& dir_path) -> std::vector<std::string> {
    std::vector<std::string> file_paths;
    try {
        if (!native_fs::exists(dir_path)) {
            LOG_ERROR(atom::log::utilities::Packager, "Directory does not exist:" + dir_path);
            return file_paths;
        }
        if (!native_fs::is_directory(dir_path)) {
            LOG_ERROR(atom::log::utilities::Packager, "Not a valid directory:" + dir_path);
            return file_paths;
        }

        // Recursively traverse directories, collecting only regular files
        for (const auto& entry : native_fs::recursive_directory_iterator(dir_path)) {
            if (native_fs::is_regular_file(entry)) {
                file_paths.push_back(entry.path().string());
            }
        }
    } catch (const native_fs::filesystem_error& e) {
        LOG_ERROR(atom::log::utilities::Packager,
                  "Directory traversal failed:" + dir_path + " -> " + e.what());
    }
    return file_paths;
}

// Traverse multiple directories, collecting all file paths
static auto TraverseMultipleDirectories(const std::vector<std::string>& dir_paths) -> std::vector<std::string> {
    std::vector<std::string> all_file_paths;
    for (const auto& dir : dir_paths) {
        auto single_dir_files = TraverseSingleDirectory(dir);
        // Merge file paths from a single directory into the total list
        all_file_paths.insert(all_file_paths.end(), single_dir_files.begin(), single_dir_files.end());
    }
    return all_file_paths;
}

// Parse user input for multiple directories
static auto ParseMultiDirectories(const std::string& input) -> std::vector<std::string> {
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
static auto PackFiles(const std::string& packName, const std::vector<std::string>& resourcePath) -> bool {
    Packager packer;
    Packager::Config config;
    config.verbose = true;

    LOG_INFO(atom::log::utilities::Packager,
             "Commencing packing... Total number of files awaiting packing:" + std::to_string(resourcePath.size()));
    const auto result = packer.Pack(resourcePath, packName, config);

    if (result == Packager::Result::Success) {
        LOG_INFO(atom::log::utilities::Packager, "Packing successful!");
        packer.PrintPackageInfo();
        return true;
    } else {
        LOG_ERROR(atom::log::utilities::Packager, "Packing failed!");
        return false;
    }
}

// Original unpacking function
static auto UnpackAllToFolder(const std::string& packName) -> bool {
    Unpackager unpacker;
    if (unpacker.Load(packName) == Unpackager::Result::Success) {
        unpacker.PrintPackageInfo();
        // Unpack to disk
        Unpackager::Config unpackConfig;
        unpackConfig.verbose = true;
        unpackConfig.outputDir = "extract/";
        unpackConfig.preserveStructure = true;
        const auto result = unpacker.UnpackAll(unpackConfig);
        if (result == Unpackager::Result::Success) {
            LOG_INFO(atom::log::utilities::Packager, "Unpacking successful!");
            return true;
        }
        LOG_ERROR(atom::log::utilities::Packager, "Unpacking failed while extracting files!");
        return false;
    } else {
        LOG_ERROR(atom::log::utilities::Packager, "Unpacking failed!");
        return false;
    }
}

// ---------------------------------------------------------------------------
// APKG v2 subcommand: `packager_tool pack2 <input> <output.apkg> [package_path]`
//   <input>        a directory (recursed) or a single file
//   <output.apkg>  output package path
//   [package_path] required for single files (their path inside the package);
//                  for directories it is an optional prefix
// ---------------------------------------------------------------------------
static auto PrintBuilderResult(const PackageBuilder::Result result) -> const char* {
    switch (result) {
    case PackageBuilder::Result::Success:
        return "success";
    case PackageBuilder::Result::ErrorOpenOutput:
        return "cannot open output file";
    case PackageBuilder::Result::ErrorReadFile:
        return "failed reading an input file";
    case PackageBuilder::Result::ErrorInvalidPath:
        return "invalid path";
    case PackageBuilder::Result::ErrorEmptyPackage:
        return "no files to pack";
    case PackageBuilder::Result::ErrorWriteFailed:
        return "write failed";
    case PackageBuilder::Result::ErrorDuplicatePath:
        return "duplicate package path";
    case PackageBuilder::Result::ErrorVerificationFailed:
        return "verification failed";
    }
    return "unknown error";
}

static auto RunPackV2(const std::vector<std::string>& args) -> int {
    if (args.size() < 3 || args.size() > 4) {
        std::cerr << "usage: packager_tool pack2 <input> <output.apkg> [package_path]\n";
        return 2;
    }
    const std::string& input = args[1];
    const std::string& output = args[2];
    const std::string package_path = args.size() == 4 ? args[3] : "";

    PackageBuilder builder;
    std::error_code error;
    const native_fs::path input_path = atom::PathFromUtf8(input);

    PackageBuilder::Result add_result = PackageBuilder::Result::ErrorInvalidPath;
    if (native_fs::is_directory(input_path, error) && !error) {
        add_result = builder.AddDirectory(input, package_path);
    } else if (native_fs::is_regular_file(input_path, error) && !error) {
        if (package_path.empty()) {
            std::cerr << "a single-file input requires an explicit package_path\n";
            return 2;
        }
        add_result = builder.AddFile(input, package_path);
    }
    if (add_result != PackageBuilder::Result::Success) {
        std::cerr << "failed to queue input: " << PrintBuilderResult(add_result) << "\n";
        return 1;
    }

    PackageBuilder::Config config;
    config.verbose = true;
    const PackageBuilder::Result build_result = builder.Build(output, config);
    if (build_result == PackageBuilder::Result::Success) {
        LOG_INFO(atom::log::utilities::Packager, "APKG v2 packing successful!");
        std::cout << "Packed " << builder.PlannedFiles().size() << " file(s) into " << output << "\n";
        return 0;
    }
    std::cerr << "APKG v2 packing failed: " << PrintBuilderResult(build_result) << "\n";
    return 1;
}

static auto PrintUsage() -> void {
    std::cout << "Atom Resource Package / Unpackage Tools\n"
              << "usage:\n"
              << "  packager_tool                      interactive menu (APKG v1)\n"
              << "  packager_tool pack2 <input> <out>  build an APKG v2 package\n";
}

auto main(int argc, char** argv) -> int {
    atom::Log::SetConsoleOutputUtf8();
    const std::vector<std::string> args = CollectUtf8Args(argc, argv);

    if (args.size() > 1) {
        if (args[1] == "pack2")
            return RunPackV2(std::vector<std::string>(args.begin() + 1, args.end()));
        if (args[1] == "--help" || args[1] == "-h") {
            PrintUsage();
            return 0;
        }
        PrintUsage();
        return 2;
    }

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
            LOG_ERROR(atom::log::utilities::Packager, "Please enter a valid number (0-2)!");
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
                LOG_ERROR(atom::log::utilities::Packager, "Error: No valid directories entered!");
                break;
            }
            LOG_INFO(atom::log::utilities::Packager,
                     "Successfully parsed " + std::to_string(dir_paths.size()) + " directories to pack.");

            // 2. Traverse multiple directories, collecting all file paths
            std::vector<std::string> all_files = TraverseMultipleDirectories(dir_paths);
            if (all_files.empty()) {
                LOG_ERROR(atom::log::utilities::Packager,
                          "Error: No files found in the entered directories!");
                break;
            }

            // 3. Call the packing function
            PackFiles(pack_name, all_files);
            break;
        }
        case 2: {
            std::string pack_name;
            LOG_INFO(atom::log::utilities::Packager,
                     "\nPlease enter the name of the package file to be unpacked (e.g., media_res.dat):");
            std::getline(std::cin, pack_name);
            UnpackAllToFolder(pack_name);
            break;
        }
        case 0: {
            LOG_INFO(atom::log::utilities::Packager, "\nExited. Goodbye!");
            return 0;
        }
        default:
            LOG_ERROR(atom::log::utilities::Packager,
                      "Invalid option, please enter a number between 0 and 2!");
            break;
        }

        std::cout << "\n-------------------------------------" << std::endl;
        std::cout << "Press Enter to continue...";
        std::cin.get();
    }
}
