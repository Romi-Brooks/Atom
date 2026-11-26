#include <Atom/AtomMain.hpp>

#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include <windows.h>
#include <shellapi.h>

auto WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) -> int {
    int argc = 0;
    const std::unique_ptr<wchar_t*, decltype(&LocalFree)> wide_arguments{CommandLineToArgvW(GetCommandLineW(), &argc),
                                                                         &LocalFree};
    if (!wide_arguments) {
        return EXIT_FAILURE;
    }

    std::vector<std::string> arguments;
    arguments.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index) {
        const auto* wide = wide_arguments.get()[index];
        const int size = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
        if (size == 0) {
            return EXIT_FAILURE;
        }

        auto& argument = arguments.emplace_back(static_cast<std::size_t>(size), '\0');
        if (WideCharToMultiByte(CP_UTF8, 0, wide, -1, argument.data(), size, nullptr, nullptr) == 0) {
            return EXIT_FAILURE;
        }
        argument.pop_back();
    }

    std::vector<char*> argv;
    argv.reserve(arguments.size() + 1);
    for (auto& argument : arguments) {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);
    return atom::platform::ApplicationMain(argc, argv.data());
}
