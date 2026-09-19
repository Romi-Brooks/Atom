#include <Test/Support/TestHelpers.hpp>
#include <Utilities/Packager/Packager.hpp>
#include <Utilities/Packager/Unpackager.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

namespace native_fs = std::filesystem;

class TemporaryDirectory final {
    public:
        [[nodiscard]] static auto Create(TemporaryDirectory& output) -> bool {
            std::error_code error;
            const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
            const auto root = native_fs::temp_directory_path(error) / ("atom_packager_test_" + std::to_string(nonce));
            if (error || native_fs::exists(root, error))
                return false;
            if (!native_fs::create_directories(root, error) || error)
                return false;
            output.root_ = root;
            return true;
        }

        ~TemporaryDirectory() {
            if (!root_.empty()) {
                std::error_code error;
                native_fs::remove_all(root_, error);
            }
        }

        TemporaryDirectory() = default;
        TemporaryDirectory(const TemporaryDirectory&) = delete;
        auto operator=(const TemporaryDirectory&) -> TemporaryDirectory& = delete;

        [[nodiscard]] auto Root() const -> const native_fs::path& { return root_; }

    private:
        native_fs::path root_{};
};

auto WriteText(const native_fs::path& path, const std::string& text) -> bool {
    std::ofstream stream(path, std::ios::binary);
    if (!stream)
        return false;
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(stream);
}

} // namespace

auto main() -> int {
    TemporaryDirectory temp{};
    ATOM_CHECK(TemporaryDirectory::Create(temp));

    const auto input_dir = temp.Root() / "in";
    std::error_code error{};
    native_fs::create_directories(input_dir / "nested", error);
    ATOM_CHECK(!error);
    ATOM_CHECK(WriteText(input_dir / "a.txt", "alpha-content"));
    ATOM_CHECK(WriteText(input_dir / "nested" / "b.txt", "beta-content"));

    const auto package_path = (temp.Root() / "demo.apkg").string();

    atom::tools::Packager packager{};
    atom::tools::Packager::Config pack_config{};
    pack_config.compress = false;
    pack_config.verbose = false;
    pack_config.preserveStructure = true;
    pack_config.overwrite = true;
    ATOM_CHECK(packager.Pack({input_dir.string()}, package_path, pack_config) ==
               atom::tools::Packager::Result::SUCCESS);
    ATOM_CHECK(native_fs::exists(package_path));
    ATOM_CHECK(packager.GetPackedFiles().size() >= 2);

    // Empty pack is a controlled failure.
    atom::tools::Packager empty_packager{};
    const auto empty_path = (temp.Root() / "empty.apkg").string();
    ATOM_CHECK(empty_packager.Pack({}, empty_path, pack_config) == atom::tools::Packager::Result::ERROR_EMPTY_PACKAGE);

    atom::tools::Unpackager unpackager{};
    ATOM_CHECK(unpackager.Load(package_path, false) == atom::tools::Unpackager::Result::SUCCESS);
    const auto list = unpackager.GetFileList();
    ATOM_CHECK(list.size() >= 2);
    ATOM_CHECK(!unpackager.Contains("definitely-missing.txt"));

    bool found_alpha = false;
    bool found_beta = false;
    for (const auto& name : list) {
        if (name.find("a.txt") != std::string::npos) {
            auto file = unpackager.ExtractFileToMemory(name);
            ATOM_CHECK(file != nullptr);
            ATOM_CHECK(file->ToString() == "alpha-content");
            found_alpha = true;
        }
        if (name.find("b.txt") != std::string::npos) {
            auto file = unpackager.ExtractFileToMemory(name);
            ATOM_CHECK(file != nullptr);
            ATOM_CHECK(file->ToString() == "beta-content");
            found_beta = true;
        }
    }
    ATOM_CHECK(found_alpha);
    ATOM_CHECK(found_beta);

    std::vector<atom::tools::Unpackager::MemoryFile> all{};
    ATOM_CHECK(unpackager.ExtractAllToMemory(all) == atom::tools::Unpackager::Result::SUCCESS);
    ATOM_CHECK(all.size() >= 2);

    atom::tools::Unpackager::MemoryFile missing{};
    ATOM_CHECK(unpackager.ExtractFileToMemory("no/such.txt", missing) != atom::tools::Unpackager::Result::SUCCESS);

    // Corrupt magic is rejected.
    const auto corrupt_path = temp.Root() / "corrupt.apkg";
    WriteText(corrupt_path, "NOTAPKG-garbage-bytes-xxxxxxxxxxxx");
    atom::tools::Unpackager corrupt{};
    ATOM_CHECK(corrupt.Load(corrupt_path.string(), false) != atom::tools::Unpackager::Result::SUCCESS);
    return 0;
}
