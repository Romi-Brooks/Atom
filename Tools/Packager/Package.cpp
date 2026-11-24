#include "PackagerOperations.hpp"
#include "Utf8Arguments.hpp"
#include "PackagerGui.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include <Filesystem/PackageFormat.hpp>
#include <Log/LogSystem.hpp>
#include <Utilities/Utf8/Utf8.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

namespace packager = atom::packager;
namespace pf = atom::fs::package_format;

auto ReadLine(std::string_view prompt) -> std::optional<std::string> {
    std::cout << prompt << std::flush;
#ifdef _WIN32
    const auto input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(input, &mode)) {
        std::wstring line;
        wchar_t buffer[256];
        DWORD count = 0;
        while (ReadConsoleW(input, buffer, static_cast<DWORD>(std::size(buffer)), &count, nullptr) && count > 0) {
            line.append(buffer, count);
            if (line.find(L'\n') != std::wstring::npos) {
                while (!line.empty() && (line.back() == L'\n' || line.back() == L'\r'))
                    line.pop_back();
                return atom::Utf8FromWide(line);
            }
        }
        return std::nullopt;
    }
#endif
    std::string line;
    if (!std::getline(std::cin, line))
        return std::nullopt;
    if (!line.empty() && line.back() == '\r')
        line.pop_back();
    return line;
}

auto PrintAssets(const std::vector<atom::fs::PackagedAssetInfo>& assets, std::string_view filter,
                 bool paginate) -> void {
    std::size_t count = 0;
    for (const auto& asset : assets) {
        if (asset.path.find(filter) == std::string::npos)
            continue;
        if (paginate && count != 0 && count % 20 == 0) {
            const auto next = ReadLine("Enter for more, q to stop: ");
            if (!next || *next == "q")
                return;
        }
        std::cout << std::left << std::setw(9) << pf::KindName(asset.kind)
                  << std::right << std::setw(12) << asset.size << "  " << asset.path << '\n';
        ++count;
    }
    std::cout << count << " resource(s)\n";
}

auto ReportTerminalResult(const packager::OperationResult& result, bool show_success = false) -> void {
    if (atom::Log::IsConsoleSinkEnabled())
        return;
    if (result.exit_code != 0)
        std::cerr << result.message << '\n';
    else if (show_success)
        std::cout << result.message << '\n';
}

auto RunTui(const std::string& initial = {}) -> int {
    std::string current_path;
    std::vector<atom::fs::PackagedAssetInfo> assets;
    std::vector<std::string> directories;
    if (!initial.empty()) {
        auto result = packager::Inspect(initial);
        ReportTerminalResult(result);
        if (result.exit_code != 0)
            return result.exit_code;
        current_path = initial;
        assets = std::move(result.assets);
        directories = std::move(result.directories);
    }
    while (true) {
        std::cout << "\n== Atom Packager | APKG v2 ==\n";
        if (!current_path.empty())
            std::cout << current_path << "  [" << assets.size() << " resources]\n";
        const auto choice = ReadLine("[O] Open  [B] Build  [D] Directories  [L] List  [F] Find  [V] Verify  [Q] Quit\n> ");
        if (!choice || *choice == "q" || *choice == "Q")
            return 0;
        if (*choice == "o" || *choice == "O") {
            const auto path = ReadLine("Package path: ");
            if (!path)
                return 0;
            auto result = packager::Inspect(*path);
            ReportTerminalResult(result);
            if (result.exit_code == 0) {
                current_path = *path;
                assets = std::move(result.assets);
                directories = std::move(result.directories);
            }
        } else if (*choice == "b" || *choice == "B") {
            const auto input = ReadLine("Input file or folder: ");
            const auto output = ReadLine("Output .apkg: ");
            const auto prefix = ReadLine("Package path or prefix (optional for folders): ");
            const auto overwrite = ReadLine("Replace an existing package? [y/N]: ");
            if (!input || !output || !prefix || !overwrite)
                return 0;
            auto result = packager::Build({*input, *output, *prefix, *overwrite == "y" || *overwrite == "Y"});
            ReportTerminalResult(result, true);
            if (result.exit_code == 0) {
                current_path = *output;
                assets = std::move(result.assets);
                directories = std::move(result.directories);
            }
        } else if (*choice == "v" || *choice == "V") {
            if (current_path.empty())
                std::cout << "Open a package first.\n";
            else
                ReportTerminalResult(packager::Inspect(current_path, true), true);
        } else if (*choice == "d" || *choice == "D") {
            std::cout << "/\n";
            for (const auto& path : directories) {
                const auto depth = std::ranges::count(path, '/');
                std::cout << std::string(2 + 2 * static_cast<std::size_t>(depth), ' ')
                          << pf::PackagePathName(path) << "/\n";
            }
        } else if (*choice == "l" || *choice == "L") {
            PrintAssets(assets, {}, true);
        } else if (*choice == "f" || *choice == "F") {
            const auto filter = ReadLine("Path contains: ");
            if (!filter)
                return 0;
            PrintAssets(assets, *filter, true);
        }
    }
}

auto PrintUsage() -> void {
    std::cout << "Atom Resource Package Tool\n"
              << "  packager_tool pack <input> <output.apkg> [package_path]\n"
              << "  packager_tool list <package.apkg>\n"
              << "  packager_tool verify <package.apkg>\n"
              << "  packager_tool [--open <package.apkg>]  (GUI)\n"
              << "  packager_tool --tui | --path <package.apkg>\n";
}

} // namespace

auto main(int argc, char** argv) -> int {
    atom::Log::SetConsoleOutputUtf8();
    const auto args = packager::CollectUtf8Args(argc, argv);
    if (args.size() > 1 && args[1] == "pack") {
        if (args.size() != 4 && args.size() != 5) {
            PrintUsage();
            return 2;
        }
        const auto result = packager::Build({args[2], args[3], args.size() == 5 ? args[4] : "", true, true});
        atom::Log::FlushConsole();
        ReportTerminalResult(result);
        if (result.exit_code == 0)
            std::cout << "Packed " << result.assets.size() << " file(s) into " << args[3] << '\n';
        return result.exit_code;
    }
    if (args.size() == 3 && (args[1] == "list" || args[1] == "verify")) {
        const auto result = packager::Inspect(args[2], args[1] == "verify");
        atom::Log::FlushConsole();
        ReportTerminalResult(result, args[1] == "verify");
        if (result.exit_code == 0 && args[1] == "list")
            PrintAssets(result.assets, {}, false);
        return result.exit_code;
    }
    if (args.size() == 2 && args[1] == "--tui")
        return RunTui();

    if (args.size() == 3 && args[1] == "--path")
        return RunTui(args[2]);
    if (args.size() <= 1 || args[1] == "--open" || args[1].starts_with("--frames="))
        return packager::RunGui(args);

    PrintUsage();
    return (args.size() == 2 && (args[1] == "--help" || args[1] == "-h")) ? 0 : 2;
}
