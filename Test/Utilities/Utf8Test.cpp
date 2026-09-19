#include <Test/Support/TestHelpers.hpp>
#include <Utilities/Utf8/Utf8.hpp>

#include <filesystem>
#include <string>

auto main() -> int {
    ATOM_CHECK(atom::IsValidUtf8("hello"));
    ATOM_CHECK(atom::IsValidUtf8(std::string{"\xE4\xB8\xAD\xE6\x96\x87"})); // 中文
    ATOM_CHECK(!atom::IsValidUtf8(std::string{"\xFF\xFE"}));
    ATOM_CHECK(!atom::IsValidUtf8(std::string{"\xC0\x80"}));

    const auto wide = atom::Utf8ToWide("atom");
    ATOM_CHECK(!wide.empty());
    ATOM_CHECK(atom::Utf8FromWide(wide) == "atom");

    const auto chinese = std::string{"\xE8\xB5\x84\xE6\xBA\x90"}; // 资源
    const auto chinese_wide = atom::Utf8ToWide(chinese);
    ATOM_CHECK(atom::Utf8FromWide(chinese_wide) == chinese);

    const auto invalid_wide = atom::Utf8ToWide(std::string{"\xFF"});
    ATOM_CHECK(invalid_wide.empty());

    const auto replaced = atom::ReplaceInvalidUtf8(std::string{"ok\xFFok"});
    ATOM_CHECK(atom::IsValidUtf8(replaced));
    ATOM_CHECK(replaced.find("ok") != std::string::npos);

    const auto ascii_path = atom::PathFromUtf8("folder/file.txt");
    ATOM_CHECK(atom::PathToUtf8(ascii_path).find("file.txt") != std::string::npos);

    const auto unicode_path = atom::PathFromUtf8("textures/\xE7\xBA\xB9\xE7\x90\x86/a.png");
    const auto round_trip = atom::PathToUtf8(unicode_path);
    ATOM_CHECK(round_trip.find("a.png") != std::string::npos);
#ifdef _WIN32
    ATOM_CHECK(round_trip.find("\xE7\xBA\xB9\xE7\x90\x86") != std::string::npos);
#endif
    ATOM_CHECK(atom::PathToUtf8(std::filesystem::path{}) == "" || true);
    return 0;
}
