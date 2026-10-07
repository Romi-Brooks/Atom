/**
  * @file           : PackagerGui.cpp
  * @brief          : Standalone ImGui-based packager tool (APKG v1/v2).
  * @attention      : A single full-window tool (no floating ImGui windows):
  *                     - A deterministic full-viewport shell, fixed in place
  *                       (no move/resize/titlebar), independent of imgui.ini.
  *                     - One tab bar: "Open Package" (browse a package tree
  *                       with per-kind coloring) and "New Package" (pick a
  *                       source directory and build a v2 package).
  *                   Reuses the engine's DebugPanel overlay + SDL_GPU ImGui
  *                   backend, so no custom window loop is needed.
  * @date           : 2026/10/7
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

// Standard Library
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Engine Headers
#include <Window/ScreenManager.hpp>
#include <Window/RenderWindow.hpp>
#include <Window/Screen.hpp>
#include <Debugger/Overlay.hpp>
#include <Log/LogSystem.hpp>

#include <Utilities/Packager/PackageBuilder.hpp>
#include <Utilities/Utf8/Utf8.hpp>
#include <Filesystem/PackageFileSystem.hpp>
#include <Filesystem/PackageFormatV2.hpp>

namespace {

namespace native_fs = std::filesystem;
namespace pf2 = atom::fs::package_format_v2;

[[nodiscard]] auto FsResultName(atom::fs::Result result) -> const char* {
    switch (result) {
    case atom::fs::Result::Success: return "Success";
    case atom::fs::Result::InvalidPath: return "InvalidPath";
    case atom::fs::Result::NotFound: return "NotFound";
    case atom::fs::Result::NotFile: return "NotFile";
    case atom::fs::Result::NotDirectory: return "NotDirectory";
    case atom::fs::Result::OutsideRoot: return "OutsideRoot";
    case atom::fs::Result::OutOfRange: return "OutOfRange";
    case atom::fs::Result::IoError: return "IoError";
    }
    return "Unknown";
}

[[nodiscard]] auto BuildResultName(atom::tools::PackageBuilder::Result result) -> const char* {
    switch (result) {
    case atom::tools::PackageBuilder::Result::Success: return "Success";
    case atom::tools::PackageBuilder::Result::ErrorOpenOutput: return "OpenOutput";
    case atom::tools::PackageBuilder::Result::ErrorReadFile: return "ReadFile";
    case atom::tools::PackageBuilder::Result::ErrorInvalidPath: return "InvalidPath";
    case atom::tools::PackageBuilder::Result::ErrorEmptyPackage: return "EmptyPackage";
    case atom::tools::PackageBuilder::Result::ErrorWriteFailed: return "WriteFailed";
    case atom::tools::PackageBuilder::Result::ErrorDuplicatePath: return "DuplicatePath";
    case atom::tools::PackageBuilder::Result::ErrorVerificationFailed: return "VerificationFailed";
    }
    return "Unknown";
}

[[nodiscard]] auto KindColor(std::uint16_t kind) -> ImVec4 {
    switch (kind) {
    case pf2::asset_kind::texture: return {0.35f, 0.90f, 0.45f, 1.0f};
    case pf2::asset_kind::audio: return {0.45f, 0.65f, 1.00f, 1.0f};
    case pf2::asset_kind::shader: return {0.80f, 0.50f, 1.00f, 1.0f};
    case pf2::asset_kind::config: return {1.00f, 0.85f, 0.35f, 1.0f};
    case pf2::asset_kind::script: return {0.35f, 0.90f, 0.90f, 1.0f};
    case pf2::asset_kind::font: return {1.00f, 0.45f, 0.80f, 1.0f};
    case pf2::asset_kind::model: return {1.00f, 0.60f, 0.30f, 1.0f};
    default: return {0.65f, 0.65f, 0.65f, 1.0f}; // binary / unknown
    }
}

[[nodiscard]] auto FormatSize(std::uint64_t bytes) -> std::string {
    char buffer[64]{};
    if (bytes >= 1024 * 1024)
        std::snprintf(buffer, sizeof(buffer), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    else if (bytes >= 1024)
        std::snprintf(buffer, sizeof(buffer), "%.1f KB", static_cast<double>(bytes) / 1024.0);
    else
        std::snprintf(buffer, sizeof(buffer), "%llu B", static_cast<unsigned long long>(bytes));
    return buffer;
}

// One node in the in-memory package tree shown by the browser.
struct TreeNode {
        std::string name{};
        bool is_dir = false;
        std::uint16_t kind = pf2::asset_kind::unknown;
        std::uint64_t size = 0;
        std::map<std::string, TreeNode> children{};
};

auto InsertPath(TreeNode& root, const atom::fs::PackagedAssetInfo& asset) -> void {
    TreeNode* node = &root;
    std::size_t start = 0;
    for (;;) {
        const std::size_t slash = asset.path.find('/', start);
        if (slash == std::string::npos) {
            TreeNode& leaf = node->children[asset.path.substr(start)];
            leaf.name = asset.path.substr(start);
            leaf.is_dir = false;
            leaf.kind = asset.kind;
            leaf.size = asset.size;
            return;
        }
        TreeNode& dir = node->children[asset.path.substr(start, slash - start)];
        dir.name = asset.path.substr(start, slash - start);
        dir.is_dir = true;
        node = &dir;
        start = slash + 1;
    }
}

class PackagerScreen final : public atom::Screen {
    public:
        auto Render(atom::render::IRenderDevice& device) -> void override {
            device.Clear(atom::color::Color::Black());
        }

        auto HandleEvent(const atom::window::IEvent& event) -> bool override {
            if (event.type == atom::window::EventType::KeyPressed) {
                const auto& key = std::get<atom::window::KeyEvent>(event.data);
                if (key.key == atom::event::Key::Escape) {
                    atom::RenderWindow::GetInstance().Shutdown();
                    return true;
                }
            }
            return false;
        }

        auto Update(float) -> void override {}
};

class PackagerPanel final : public atom::debugger::DebugPanel {
    public:
        PackagerPanel() : DebugPanel("Packager") {
            browser_dir_ = atom::PathToUtf8(native_fs::current_path());
        }

    private:
        // ImGui::InputText requires a persistent buffer across frames; these
        // member arrays keep the edit state stable (a per-frame stack buffer
        // would reset the text every frame).
        char package_path_buf_[1024]{};
        char source_dir_buf_[1024]{};
        char output_path_buf_[1024]{};
        char prefix_buf_[256]{};

        // --- Open Package state ---
        std::unique_ptr<atom::fs::PackageFileSystem> package_{};
        std::vector<atom::fs::PackagedAssetInfo> assets_{};
        TreeNode tree_root_{};
        std::string browse_status_{"No package loaded"};

        // --- New Package state ---
        std::string pack_status_{};

        // --- Native directory browser state ---
        struct BrowserEntry {
                std::string name{};
                std::string path{};
        };
        bool show_browser_ = false;
        std::string browser_dir_{};
        std::string browser_last_dir_{};
        std::vector<BrowserEntry> browser_entries_{};

        auto OpenPackage() -> void {
            package_.reset();
            assets_.clear();
            tree_root_ = TreeNode{};

            const std::string path = package_path_buf_;
            if (path.empty()) {
                browse_status_ = "Enter a package path first";
                return;
            }

            std::unique_ptr<atom::fs::PackageFileSystem> package{};
            const atom::fs::Result result = atom::fs::PackageFileSystem::Create("res", path, package);
            if (result != atom::fs::Result::Success) {
                browse_status_ = std::string{"Open failed: "} + FsResultName(result);
                return;
            }
            std::vector<atom::fs::PackagedAssetInfo> list{};
            if (package->EnumerateAssets(list) != atom::fs::Result::Success) {
                browse_status_ = "Enumerate failed";
                return;
            }
            package_ = std::move(package);
            assets_ = std::move(list);
            for (const auto& asset : assets_)
                InsertPath(tree_root_, asset);
            browse_status_ = "Loaded " + std::to_string(assets_.size()) + " assets";
        }

        auto DoPack() -> void {
            pack_status_.clear();
            const std::string source = source_dir_buf_;
            const std::string output = output_path_buf_;
            if (source.empty() || output.empty()) {
                pack_status_ = "Source directory and output path are both required";
                return;
            }

            atom::tools::PackageBuilder builder{};
            atom::tools::PackageBuilder::Config config{};
            config.verbose = true;
            config.overwrite = true;

            const std::string prefix = prefix_buf_;
            const atom::tools::PackageBuilder::Result add =
                builder.AddDirectory(source, prefix.empty() ? std::string{} : prefix);
            if (add != atom::tools::PackageBuilder::Result::Success) {
                pack_status_ = std::string{"AddDirectory failed: "} + BuildResultName(add);
                return;
            }
            const std::size_t count = builder.PlannedFiles().size();
            const atom::tools::PackageBuilder::Result build = builder.Build(output, config);
            if (build != atom::tools::PackageBuilder::Result::Success) {
                pack_status_ = std::string{"Build failed: "} + BuildResultName(build);
                return;
            }
            pack_status_ = "Packed " + std::to_string(count) + " files -> " + output;
            std::snprintf(package_path_buf_, sizeof(package_path_buf_), "%s", output.c_str());
            OpenPackage();
        }

        auto RefreshBrowser() -> void {
            browser_entries_.clear();
            const native_fs::path dir = atom::PathFromUtf8(browser_dir_);
            std::error_code error{};
            for (const auto& entry : native_fs::directory_iterator(dir, error)) {
                if (error)
                    break;
                std::error_code child_error{};
                if (!entry.is_directory(child_error) || child_error)
                    continue;
                BrowserEntry item{};
                item.name = atom::PathToUtf8(entry.path().filename());
                item.path = atom::PathToUtf8(entry.path());
                browser_entries_.push_back(std::move(item));
            }
            std::ranges::sort(browser_entries_,
                              [](const BrowserEntry& a, const BrowserEntry& b) { return a.name < b.name; });
        }

        auto DrawBrowser() -> void {
            if (browser_dir_ != browser_last_dir_) {
                RefreshBrowser();
                browser_last_dir_ = browser_dir_;
            }
            ImGui::TextDisabled("Current: %s", browser_dir_.c_str());
            ImGui::Separator();

            const native_fs::path parent = atom::PathFromUtf8(browser_dir_).parent_path();
            if (!parent.empty() && ImGui::Button(".. (up)")) {
                browser_dir_ = atom::PathToUtf8(parent);
                return;
            }
            ImGui::SameLine();
            if (ImGui::Button("Use this directory"))
                std::snprintf(source_dir_buf_, sizeof(source_dir_buf_), "%s", browser_dir_.c_str());
            ImGui::Separator();

            for (const auto& entry : browser_entries_) {
                if (ImGui::Selectable(entry.name.c_str())) {
                    browser_dir_ = entry.path;
                    return;
                }
            }
        }

        auto DrawNode(const TreeNode& node) -> void {
            if (node.is_dir) {
                if (ImGui::TreeNode(node.name.c_str())) {
                    for (const auto& [name, child] : node.children)
                        DrawNode(child);
                    ImGui::TreePop();
                }
                return;
            }
            ImGui::TextColored(KindColor(node.kind), "%s", node.name.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("  %s  %s", pf2::KindName(node.kind), FormatSize(node.size).c_str());
        }

        auto DrawOpenTab() -> void {
            ImGui::InputText("Package path", package_path_buf_, sizeof(package_path_buf_));
            ImGui::SameLine();
            if (ImGui::Button("Open"))
                OpenPackage();
            ImGui::Separator();

            if (!package_) {
                ImGui::TextDisabled("%s", browse_status_.c_str());
                return;
            }
            ImGui::TextDisabled("%s", browse_status_.c_str());
            ImGui::Separator();
            for (const auto& [name, child] : tree_root_.children)
                DrawNode(child);
        }

        auto DrawNewTab() -> void {
            ImGui::InputText("Source directory", source_dir_buf_, sizeof(source_dir_buf_));
            ImGui::SameLine();
            if (ImGui::Button("Browse..."))
                show_browser_ = !show_browser_;
            if (show_browser_) {
                ImGui::Indent();
                DrawBrowser();
                ImGui::Unindent();
            }

            ImGui::InputText("Output .apkg", output_path_buf_, sizeof(output_path_buf_));
            ImGui::InputText("Package prefix (optional)", prefix_buf_, sizeof(prefix_buf_));

            if (ImGui::Button("Pack"))
                DoPack();
            if (!pack_status_.empty())
                ImGui::TextWrapped("%s", pack_status_.c_str());
        }

    protected:
        auto OnDrawOverlay() -> void override {
            // Deterministic full-window shell: fixed in place, follows the main
            // viewport, and never relies on imgui.ini (mirrors PanelLayout's
            // "declared default geometry" convention).
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::Begin("Atom Packager", nullptr,
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBringToFrontOnFocus);

            if (ImGui::BeginTabBar("##packager_tabs")) {
                if (ImGui::BeginTabItem("Open Package")) {
                    DrawOpenTab();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("New Package")) {
                    DrawNewTab();
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            ImGui::End();
        }
};

} // namespace

auto main() -> int {
    atom::Log::SetConsoleOutputUtf8();
    atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_DEBUG);

    if (const auto loaded =
            atom::ScreenManager::GetInstance().LoadScreen("Packager", std::make_unique<PackagerScreen>());
        loaded != atom::ScreenOpResult::Success) {
        LOG_ERROR(atom::log::core::ScreenManager,
                  "Failed to register the packager screen: " + std::string{atom::ToString(loaded)});
        return 1;
    }
    auto* screen = atom::ScreenManager::GetInstance().GetScreen("Packager");
    atom::ScreenManager::GetInstance().SwitchScreen("Packager");

    auto& window = atom::RenderWindow::GetInstance();
    window.Initialize("Atom Packager", atom::algo::Vec2{1280, 720}, atom::backend::RenderBackendId::SdlGpu);
    window.SetFPS(60);

    PackagerPanel panel{};
    panel.Attach(window, *screen);

    window.Run();
}
