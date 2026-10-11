// Standard Library
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>

// Engine Headers
#include <Log/LogSystem.hpp>
#include <Utilities/Utf8/Utf8.hpp>

// Self Dependencies
#include "PackageBuilder.hpp"

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#endif

namespace native_fs = std::filesystem;
using atom::tools::PackageBuilder;

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

// ---------------------------------------------------------------------------
// APKG subcommand: `packager_tool pack <input> <output.apkg> [package_path]`
//   <input>        a directory (recursed) or a single file
//   <output.apkg>  output package path
//   [package_path] required for single files (their path inside the package);
//                  for directories it is an optional prefix
// ---------------------------------------------------------------------------
static auto RunPack(const std::vector<std::string>& args) -> int {
    if (args.size() < 3 || args.size() > 4) {
        std::cerr << "usage: packager_tool pack <input> <output.apkg> [package_path]\n";
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
        LOG_INFO(atom::log::utilities::Packager, "APKG packing successful!");
        std::cout << "Packed " << builder.PlannedFiles().size() << " file(s) into " << output << "\n";
        return 0;
    }
    std::cerr << "APKG packing failed: " << PrintBuilderResult(build_result) << "\n";
    return 1;
}

static auto PrintUsage() -> void {
    std::cout << "Atom Resource Package Tool\n"
              << "usage:\n"
              << "  packager_tool pack <input> <output.apkg> [package_path]\n";
}

auto main(int argc, char** argv) -> int {
    atom::Log::SetConsoleOutputUtf8();
    const std::vector<std::string> args = CollectUtf8Args(argc, argv);

    if (args.size() > 1) {
        if (args[1] == "pack")
            return RunPack(std::vector<std::string>(args.begin() + 1, args.end()));
        if (args[1] == "--help" || args[1] == "-h") {
            PrintUsage();
            return 0;
        }
        PrintUsage();
        return 2;
    }

    PrintUsage();
    return 0;
}
