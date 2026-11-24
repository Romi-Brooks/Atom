#include "Utf8Arguments.hpp"

#include <Utilities/Utf8/Utf8.hpp>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace atom::packager {

// ---------------------------------------------------------------------------
// UTF-8 command-line arguments
//
// On Windows, the CRT converts the process command line to narrow argv using
// the ANSI code page (GBK on Chinese systems), which corrupts UTF-8 paths.
// We re-parse the wide command line directly and convert to UTF-8 ourselves.
// ---------------------------------------------------------------------------
auto CollectUtf8Args(int argc, char** argv) -> std::vector<std::string> {
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

} // namespace atom::packager
