#include "PackagerOperations.hpp"
#include "PackagerGui.hpp"
#include "PackagerPreview.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <set>
#include <sstream>
#include <utility>

#include <Atom/Core/Concurrency/JobSystem.hpp>
#include <Atom/Backend/Contracts/Numeric/NumericContract.hpp>
#include <Atom/Resources/Filesystem/PackageFormat.hpp>
#include <Atom/Core/Log/LogSystem.hpp>
#include <Atom/Resources/Media/Audio/Mixing/AudioMixer.hpp>
#include <Atom/Resources/Media/Audio/Playback/MusicPlayer.hpp>
#include <Atom/Platform/FileDialogs.hpp>
#include <Atom/Platform/ApplicationPaths.hpp>
#include <Atom/Presentation/Render/Renderer2D/Renderer2D.hpp>
#include <Atom/Runtime/Application.hpp>
#include <Atom/Runtime/ScreenManager.hpp>
#include <Atom/Presentation/UI/UiTree.hpp>
#include <Atom/Core/Utilities/Utf8/Utf8.hpp>
#include <Atom/Platform/Window/RenderWindow.hpp>

namespace {

namespace fs = atom::fs;
namespace pf = fs::package_format;
namespace ops = atom::packager;
namespace ui = atom::ui;
namespace window = atom::window;
namespace platform = atom::platform;

using atom::algo::Rect;
using atom::input::Key;
using atom::render::Color;

constexpr Color background{12, 19, 26, 255};
constexpr Color panel{20, 30, 40, 255};
constexpr Color field{13, 22, 30, 255};
constexpr Color border{37, 57, 67, 255};
constexpr Color ink{232, 241, 240, 255};
constexpr Color muted{154, 176, 184, 255};
constexpr Color accent{119, 231, 197, 255};
constexpr Color selection{29, 67, 65, 255};
constexpr Color failure{255, 144, 133, 255};

auto Contains(Rect r, float x, float y) -> bool {
    return x >= r.x && y >= r.y && x < r.x + r.width && y < r.y + r.height;
}

auto ReadFont(const std::filesystem::path& path) -> std::vector<std::byte> {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    const auto size = file.tellg();
    if (size <= 0 || size > 64 * 1024 * 1024)
        return {};
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), size))
        return {};
    return bytes;
}

auto SizeLabel(std::uint64_t bytes) -> std::string {
    constexpr std::array units{"B", "KiB", "MiB", "GiB", "TiB"};
    auto size = static_cast<double>(bytes);
    std::size_t unit = 0;
    while (size >= 1024 && unit + 1 < units.size()) {
        size /= 1024;
        ++unit;
    }
    std::ostringstream text;
    text << std::fixed << std::setprecision(unit == 0 ? 0 : 1) << size << ' ' << units[unit];
    return text.str();
}

struct ProgressState {
    std::mutex mutex;
    ops::Phase phase = ops::Phase::Write;
    std::size_t completed = 0;
    std::size_t total = 0;
};

struct LogBuffer {
    std::mutex mutex;
    std::deque<atom::LogRecord> entries;
};

enum class Field { None, Archive, Search, Source, Output, Prefix };

struct DialogResult {
    std::mutex mutex;
    bool done = false;
    std::string path;
    std::string error;
};

class PackagerScreen final : public atom::Screen {
    public:
        PackagerScreen(atom::runtime::Application& app, unsigned frame_limit) : app_(app), frames_left_(frame_limit) {
            using namespace atom::layout;
            LayoutStyle row;
            row.flex_direction = FlexDirection::Row;
            row.justify_content = Justify::FlexEnd;
            row.align_items = Align::Center;
            toolbar_.SetLayoutStyle(toolbar_.Root(), row);
            const auto button = [this](std::string text, ui::ClickHandler action) {
                LayoutStyle layout;
                layout.width = Length::Points(108);
                layout.height = Length::Points(42);
                layout.margin.left = Length::Points(7);
                ui::VisualStyle style;
                style.draw_background = true;
                style.text = ink;
                style.text_size = 14;
                style.background = border;
                style.hover_background = selection;
                style.pressed_background = field;
                return toolbar_.CreateButton(toolbar_.Root(), std::move(text), layout, style, std::move(action));
            };
            browse_button_ = button("Browse", [this] { SetMode(false); });
            build_button_ = button("Build", [this] { SetMode(true); });
            button("Activity", [this] { show_log_ = !show_log_; });

            log_connection_ = atom::Log::Subscribe([sink = logs_](const atom::LogRecord& record) {
                if (record.channel_prefix != "Atom.Utilities." || record.channel_name != "Packager")
                    return;
                const std::scoped_lock lock{sink->mutex};
                if (sink->entries.size() == 300)
                    sink->entries.pop_front();
                sink->entries.push_back(record);
            });
        }

        auto Initialize(atom::render::IRenderDevice& device) -> bool {
            std::filesystem::path shaders{ATOM_SHADER_OUTPUT_DIR};
            if (const auto base = platform::ApplicationBasePath()) {
                std::error_code error;
                const auto adjacent = *base / "Shaders";
                if (std::filesystem::is_directory(adjacent, error))
                    shaders = adjacent;
            }
            if (!renderer_.Initialize(device, shaders))
                return false;
#ifdef _WIN32
            font_bytes_ = ReadFont("C:/Windows/Fonts/segoeui.ttf");
            cjk_font_bytes_ = ReadFont("C:/Windows/Fonts/msyh.ttc");
#elif defined(__APPLE__)
            font_bytes_ = ReadFont("/System/Library/Fonts/Helvetica.ttc");
            cjk_font_bytes_ = ReadFont("/System/Library/Fonts/PingFang.ttc");
#else
            font_bytes_ = ReadFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
            cjk_font_bytes_ = ReadFont("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc");
#endif
            if (!font_bytes_.empty())
                font_ = renderer_.LoadFontFromMemory(font_bytes_);
            if (!cjk_font_bytes_.empty())
                cjk_font_ = renderer_.LoadFontFromMemory(cjk_font_bytes_);
            if (!font_)
                font_ = cjk_font_;
            return font_ != nullptr;
        }

        auto Shutdown() -> void {
            auto& jobs = atom::job::JobSystem::GetInstance();
            jobs.WaitFor(job_);
            jobs.WaitFor(preview_job_);
            ClearPreview();
            if (renderer_.IsInitialized())
                renderer_.Shutdown();
        }

        auto Open(std::string path, bool verify = false) -> void {
            if (path.empty() || Busy())
                return;
            active_path_ = path;
            Submit(verify ? "Verify" : "Open", [path = std::move(path), verify](const ops::Progress& progress) {
                return ops::Inspect(path, verify, progress);
            });
        }

        auto Update(float) -> void override {
            toolbar_.Update();
            if (pending_ && job_.IsComplete()) {
                message_ = pending_->message;
                failed_ = pending_->exit_code != 0;
                if (!failed_) {
                    const bool new_archive = archive_ != active_path_;
                    archive_ = active_path_;
                    archive_input_ = archive_;
                    assets_ = std::move(pending_->assets);
                    if (new_archive) {
                        selected_.clear();
                        root_expanded_ = true;
                        tree_scroll_ = 0;
                        folder_.clear();
                        expanded_.clear();
                        search_.clear();
                        kind_ = -1;
                    }
                    if (operation_ != "Verify")
                        SelectAsset(nullptr);
                    directories_ = std::move(pending_->directories);
                    while (!folder_.empty() && !std::ranges::binary_search(directories_, folder_))
                        folder_ = std::string{pf::PackagePathParent(folder_)};
                    std::erase_if(expanded_, [&](const auto& dir) {
                        return !std::ranges::binary_search(directories_, dir);
                    });
                    Refilter();
                    if (operation_ == "Build")
                        SetMode(false);
                }
                pending_.reset();
            }
            UpdatePreview();
            PollDialog();
        }

        auto HandleEvent(const window::IEvent& event) -> bool override {
            using enum window::EventType;
            if (const auto* pointer = std::get_if<window::MouseEvent>(&event.data)) {
                auto mapped = event;
                auto& coordinates = std::get<window::MouseEvent>(mapped.data);
                coordinates.x *= input_x_;
                coordinates.y *= input_y_;
                cursor_x_ = coordinates.x;
                cursor_y_ = coordinates.y;
                if (toolbar_.HandleEvent(mapped))
                    return true;
                if (event.type == MouseButtonPressed && coordinates.button == atom::input::MouseButton::Left) {
                    Focus(Field::None);
                    for (const auto& item : hits_) {
                        if (Contains(item.rect, cursor_x_, cursor_y_)) {
                            item.action();
                            return true;
                        }
                    }
                }
            }
            if (event.type == MouseWheel) {
                const auto wheel = std::get<window::MouseWheelEvent>(event.data).y;
                if (wheel != 0 && std::isfinite(wheel)) {
                    const int steps = wheel > 0 ? -3 : 3;
                    if (Contains(tree_rect_, cursor_x_, cursor_y_))
                        tree_scroll_ = std::max(0, tree_scroll_ + steps);
                    else if (Contains(list_rect_, cursor_x_, cursor_y_))
                        list_scroll_ = std::max(0, list_scroll_ + steps);
                    else if (Contains(preview_rect_, cursor_x_, cursor_y_))
                        preview_scroll_ = std::max(0, preview_scroll_ + steps);
                    else if (show_log_ && Contains(log_rect_, cursor_x_, cursor_y_))
                        log_scroll_ = std::max(0, log_scroll_ - steps);
                }
            }
            if (event.type == TextInput && focus_ != Field::None) {
                auto& value = Value(focus_);
                const auto& text = std::get<window::TextInputEvent>(event.data).text;
                if (value.size() + text.size() <= pf::max_path_length)
                    value += text;
                if (focus_ == Field::Search)
                    Refilter();
                return true;
            }
            if (event.type == KeyPressed && HandleKey(std::get<window::KeyEvent>(event.data)))
                return true;
            return event.type == KeyReleased && toolbar_.HandleEvent(event);
        }

        auto Render(atom::render::IRenderDevice& device) -> void override {
            const auto output = device.GetOutputSize();
            if (output.GetX() <= 0 || output.GetY() <= 0)
                return;
            const auto* host = app_.GetWindow().GetIWindow();
            const auto input = host ? host->GetCoordinateSize() : output;
            const float dpi = output.GetX() / std::max(1.0f, input.GetX());
            const float scale = std::min({dpi, output.GetX() / 940.0f, output.GetY() / 640.0f});
            width_ = output.GetX() / scale;
            height_ = output.GetY() / scale;
            input_x_ = output.GetX() / (std::max(1.0f, input.GetX()) * scale);
            input_y_ = output.GetY() / (std::max(1.0f, input.GetY()) * scale);
            if (!renderer_.BeginFrame(0, 0, scale)) {
                RenderFailed();
                return;
            }

            hits_.clear();
            Fill({0, 0, width_, height_}, background);
            Fill({0, 0, width_, 72}, panel);
            Fill({0, 71, width_, 1}, border);
            Fill({20, 20, 5, 33}, accent);
            Label("Atom Packager", 38, 14, 25, ink, 220);
            Label("APKG v2", 40, 45, 12, accent, 130);
            toolbar_.SetEnabled(browse_button_, !Busy());
            toolbar_.SetEnabled(build_button_, !Busy());
            toolbar_.Layout(width_ - 19, 72);
            toolbar_.Render(renderer_, font_);

            const float body_bottom = height_ - (show_log_ ? 161 : 34);
            tree_rect_ = {};
            list_rect_ = {};
            preview_rect_ = {};
            if (build_mode_)
                DrawBuild(body_bottom);
            else
                DrawBrowse(body_bottom);

            if (show_log_)
                DrawLog(body_bottom + 9);
            Fill({0, height_ - 27, width_, 27}, panel);
            std::string progress_text = operation_ + " in progress";
            if (Busy()) {
                const std::scoped_lock lock{progress_->mutex};
                if (progress_->total) {
                    const auto ratio = static_cast<float>(progress_->completed) / static_cast<float>(progress_->total);
                    Fill({0, height_ - 29, width_ * ratio, 2}, accent);
                    progress_text = (progress_->phase == ops::Phase::Write ? "Writing " : "Verifying ") +
                        std::to_string(progress_->completed) + " / " + std::to_string(progress_->total) + " resources";
                }
            }
            Label(Busy() ? progress_text : message_, 18, height_ - 23, 13,
                  failed_ ? failure : muted, width_ - 178);
            Label(Busy() ? "WORKING" : "READY", width_ - 91, height_ - 23, 12, accent, 84);
            if (!renderer_.EndFrame())
                RenderFailed();

            if (frames_left_ && --frames_left_ == 0)
                app_.RequestStop();
        }

        [[nodiscard]] auto Failed() const -> bool { return render_failed_; }

    private:
        struct Hit { Rect rect; std::function<void()> action; };

        auto Busy() const -> bool { return static_cast<bool>(pending_); }

        auto Submit(std::string operation, std::function<ops::OperationResult(const ops::Progress&)> work) -> void {
            operation_ = std::move(operation);
            failed_ = false;
            pending_ = std::make_shared<ops::OperationResult>();
            progress_ = std::make_shared<ProgressState>();
            job_ = atom::job::JobSystem::GetInstance().ScheduleOrRun("Packager: " + operation_,
                [result = pending_, progress = progress_, work = std::move(work)] {
                    try {
                        *result = work([&](ops::Phase phase, std::size_t completed, std::size_t total) {
                            const std::scoped_lock lock{progress->mutex};
                            progress->phase = phase;
                            progress->completed = completed;
                            progress->total = total;
                        });
                    } catch (const std::exception& error) {
                        *result = {{}, error.what(), 1};
                        LOG_ERROR(atom::log::utilities::Packager, result->message);
                    }
                });
        }

        auto RequestBuild() -> void {
            if (Busy() || source_.empty() || output_.empty())
                return;
            active_path_ = output_;
            Submit("Build", [request = ops::BuildRequest{source_, output_, prefix_, overwrite_}](const ops::Progress& progress) {
                return ops::Build(request, progress);
            });
        }

        auto RenderFailed() -> void {
            render_failed_ = true;
            LOG_ERROR(atom::log::utilities::Packager, "The desktop interface could not render its frame");
            app_.RequestStop();
        }

        auto SetMode(bool build) -> void {
            build_mode_ = build;
            Focus(Field::None);
        }

        auto Value(Field id) -> std::string& {
            switch (id) {
                case Field::Search: return search_;
                case Field::Source: return source_;
                case Field::Output: return output_;
                case Field::Prefix: return prefix_;
                default: return archive_input_;
            }
        }

        auto Focus(Field id) -> void {
            if (focus_ == id)
                return;
            const bool was_enabled = focus_ != Field::None;
            focus_ = id;
            if (was_enabled != (id != Field::None))
                app_.GetWindow().SetTextInputEnabled(id != Field::None);
        }

        auto HandleKey(const atom::input::KeyEvent& key) -> bool {
            if (key.key == Key::Tab && !Busy()) {
                if (build_mode_)
                    Focus(focus_ == Field::Source ? Field::Output : focus_ == Field::Output ? Field::Prefix : Field::Source);
                else
                    Focus(focus_ == Field::Archive ? Field::Search : Field::Archive);
                return true;
            }
            if (key.key == Key::Escape && focus_ != Field::None) {
                Focus(Field::None);
                return true;
            }
            if (focus_ == Field::None)
                return toolbar_.HandleEvent({window::EventType::KeyPressed, key});
            if (key.key == Key::Backspace || key.key == Key::Delete) {
                auto& value = Value(focus_);
                if (key.key == Key::Delete)
                    value.clear();
                else if (!value.empty()) {
                    auto end = value.size() - 1;
                    while (end > 0 && (static_cast<unsigned char>(value[end]) & 0xC0) == 0x80)
                        --end;
                    value.resize(end);
                }
                if (focus_ == Field::Search)
                    Refilter();
            }
            if (key.key == Key::Enter && !key.is_repeat) {
                if (build_mode_)
                    RequestBuild();
                else if (focus_ == Field::Archive)
                    Open(archive_input_);
                Focus(Field::None);
            }
            return true;
        }

        auto Pick(Field target, platform::PathDialog mode) -> void {
            if (dialog_ || Busy())
                return;
            dialog_target_ = target;
            dialog_ = std::make_shared<DialogResult>();
            platform::ShowPathDialog(mode, [result = dialog_](auto selection) {
                const std::scoped_lock lock{result->mutex};
                if (!selection)
                    result->error = selection.error().Message();
                else if (*selection)
                    result->path = std::move(**selection);
                result->done = true;
            });
        }

        auto PollDialog() -> void {
            auto reply = dialog_;
            if (!reply)
                return;
            const std::scoped_lock lock{reply->mutex};
            if (!reply->done)
                return;
            dialog_.reset();
            if (!reply->error.empty()) {
                failed_ = true;
                message_ = reply->error;
                LOG_ERROR(atom::log::utilities::Packager, message_);
            } else if (!reply->path.empty()) {
                Value(dialog_target_) = std::move(reply->path);
                if (dialog_target_ == Field::Archive)
                    Open(archive_input_);
                if (dialog_target_ == Field::Output && !atom::PathFromUtf8(output_).has_extension())
                    output_ += ".apkg";
            }
        }

        auto BrowseFolder(std::string path) -> void {
            if (path != folder_)
                SelectAsset(nullptr);
            folder_ = std::move(path);
            root_expanded_ = true;
            for (auto parent = pf::PackagePathParent(folder_); !parent.empty(); parent = pf::PackagePathParent(parent))
                expanded_.emplace(parent);
            Refilter();
        }

        auto Refilter() -> void {
            visible_.clear();
            visible_directories_.clear();
            list_scroll_ = 0;
            for (const auto& directory : directories_) {
                if (pf::PackagePathParent(directory) == folder_ && directory.find(search_) != std::string::npos)
                    visible_directories_.push_back(directory);
            }
            for (std::size_t i = 0; i < assets_.size(); ++i) {
                const auto& asset = assets_[i];
                if (pf::PackagePathParent(asset.path) != folder_)
                    continue;
                if (kind_ != -1 && asset.kind != kind_)
                    continue;
                if (asset.path.find(search_) != std::string::npos)
                    visible_.push_back(i);
            }
        }

        auto Fill(Rect rectangle, Color color) -> void { renderer_.DrawRect(rectangle, color); }

        auto Label(std::string_view value, float x, float y, float size, Color color, float max_width) -> void {
            const bool has_unicode = std::ranges::any_of(value, [](unsigned char c) { return c >= 128; });
            const auto* font = has_unicode && cjk_font_ ? cjk_font_ : font_;
            if (!font || max_width <= 0)
                return;
            const auto line_height = renderer_.MeasureText(*font, "Ag", size).height;
            renderer_.PushClip({x, y, max_width, std::max(size + 6, line_height + 3)});
            renderer_.DrawText(*font, value, x, y, color, size);
            renderer_.PopClip();
        }

        auto Button(Rect r, std::string_view label, std::function<void()> action, bool primary = false) -> void {
            const bool hot = Contains(r, cursor_x_, cursor_y_);
            Fill(r, primary ? accent : hot ? selection : border);
            const auto size = renderer_.MeasureText(*font_, label, 14);
            Label(label, r.x + std::max(8.0f, (r.width - size.width) / 2),
                  r.y + std::max(2.0f, (r.height - size.height) / 2), 14,
                  primary ? background : ink, r.width - 16);
            if (action)
                hits_.push_back({r, std::move(action)});
        }

        auto TextField(Rect r, Field id, std::string_view placeholder) -> void {
            Fill(r, field);
            renderer_.DrawRectOutline(r, focus_ == id ? accent : border);
            const auto& value = Value(id);
            Label(value.empty() ? placeholder : std::string_view{value}, r.x + 10, r.y + 7, 14,
                  value.empty() ? muted : ink, r.width - 20);
            hits_.push_back({r, [this, id] { Focus(id); }});
        }

        auto ClearPreview() -> void;
        auto SelectAsset(const fs::PackagedAssetInfo* asset) -> void;
        auto UpdatePreview() -> void;
        auto DrawPreview(Rect bounds) -> void;
        auto DrawBrowse(float bottom) -> void;
        auto DrawBuild(float bottom) -> void;
        auto DrawLog(float top) -> void;

        atom::runtime::Application& app_;
        unsigned frames_left_ = 0;
        atom::render::Renderer2D renderer_;
        atom::render::Font* font_ = nullptr;
        atom::render::Font* cjk_font_ = nullptr;
        std::vector<std::byte> font_bytes_, cjk_font_bytes_;
        ui::UiTree toolbar_;
        ui::NodeId browse_button_ = ui::invalid_node;
        ui::NodeId build_button_ = ui::invalid_node;
        std::vector<Hit> hits_;
        std::shared_ptr<LogBuffer> logs_ = std::make_shared<LogBuffer>();
        atom::LogConnection log_connection_;
        std::shared_ptr<DialogResult> dialog_;
        std::shared_ptr<ops::OperationResult> pending_;
        std::shared_ptr<ProgressState> progress_;
        atom::job::JobHandle job_;
        std::vector<fs::PackagedAssetInfo> assets_;
        std::vector<std::string> directories_;
        std::optional<fs::PackagedAssetInfo> preview_request_;
        std::shared_ptr<ops::AssetPreview> preview_pending_;
        ops::AssetPreview preview_;
        atom::job::JobHandle preview_job_;
        std::size_t preview_revision_ = 0, pending_revision_ = 0;
        atom::render::Renderer2D::Texture* preview_texture_ = nullptr;
        std::unique_ptr<atom::AudioMixer> preview_mixer_;
        std::unique_ptr<atom::MusicPlayer> preview_player_;
        Rect preview_rect_{};
        int preview_scroll_ = 0;
        bool root_expanded_ = true;
        std::set<std::string, std::less<>> expanded_;
        std::vector<std::size_t> visible_;
        std::vector<std::string_view> visible_directories_;
        std::string archive_, active_path_, archive_input_, search_, folder_, selected_, source_, output_, prefix_;
        std::string operation_, message_ = "Open an APKG archive or build one from a file or folder.";
        Field focus_ = Field::None, dialog_target_ = Field::None;
        Rect tree_rect_{}, list_rect_{}, log_rect_{};
        float width_ = 1280, height_ = 800, cursor_x_ = -1, cursor_y_ = -1, input_x_ = 1, input_y_ = 1;
        int kind_ = -1, tree_scroll_ = 0, list_scroll_ = 0, log_scroll_ = 0;
        bool build_mode_ = false, show_log_ = true, failed_ = false, render_failed_ = false, overwrite_ = false;
};
auto PackagerScreen::ClearPreview() -> void {
    preview_player_.reset();
    preview_mixer_.reset();
    if (preview_texture_) {
        renderer_.DestroyTexture(*preview_texture_);
        preview_texture_ = nullptr;
    }
    preview_ = {};
    preview_scroll_ = 0;
}

auto PackagerScreen::SelectAsset(const fs::PackagedAssetInfo* asset) -> void {
    if (asset && asset->path == selected_)
        return;
    ++preview_revision_;
    ClearPreview();
    selected_ = asset ? asset->path : std::string{};
    preview_request_.reset();
    if (asset)
        preview_request_ = *asset;
}

auto PackagerScreen::UpdatePreview() -> void {
    if (preview_pending_ && preview_job_.IsComplete()) {
        if (pending_revision_ == preview_revision_) {
            preview_ = std::move(*preview_pending_);
            if (preview_.image.IsValid()) {
                preview_texture_ = renderer_.CreateTexture(preview_.image.width, preview_.image.height,
                                                           preview_.image.rgba.data());
                if (!preview_texture_)
                    preview_.status = "Image could not be uploaded";
                preview_.image.rgba = {};
            }
        }
        preview_pending_.reset();
    }
    if (preview_pending_ || !preview_request_)
        return;
    preview_pending_ = std::make_shared<ops::AssetPreview>();
    pending_revision_ = preview_revision_;
    preview_job_ = atom::job::JobSystem::GetInstance().ScheduleOrRun("Packager: resource preview",
        [result = preview_pending_, archive = archive_, asset = std::move(*preview_request_)] {
            try {
                *result = ops::ReadPreview(archive, asset);
            } catch (const std::exception& error) {
                result->status = "Could not read preview";
                LOG_WARNING(atom::log::utilities::Packager, error.what());
            }
        });
    preview_request_.reset();
}

auto PackagerScreen::DrawPreview(Rect bounds) -> void {
    preview_rect_ = bounds;
    Fill(bounds, field);
    Label(preview_.status.empty() ? "Loading preview..." : preview_.status, bounds.x + 8, bounds.y + 7,
          13, accent, bounds.width - 16);
    if (preview_texture_) {
        const auto width = static_cast<float>(preview_.image.width);
        const auto height = static_cast<float>(preview_.image.height);
        const auto scale = std::min((bounds.width - 16) / width, (bounds.height - 39) / height);
        if (scale > 0) {
            renderer_.DrawTexture(*preview_texture_,
                {bounds.x + (bounds.width - width * scale) / 2, bounds.y + 30, width * scale, height * scale});
        }
    }
    const int rows = std::max(0, static_cast<int>((bounds.height - (preview_.audio ? 76 : 36)) / 20));
    const int count = static_cast<int>(preview_.lines.size());
    preview_scroll_ = std::clamp(preview_scroll_, 0, std::max(0, count - rows));
    for (int i = 0; i < std::min(rows, count - preview_scroll_); ++i)
        Label(preview_.lines[static_cast<std::size_t>(preview_scroll_ + i)], bounds.x + 8,
              bounds.y + 31 + i * 20, 13, ink, bounds.width - 16);

    if (preview_.audio) {
        const auto y = bounds.y + bounds.height - 37;
        Button({bounds.x + 8, y, 79, 30}, "Play", [this] {
            if (!preview_player_) {
                preview_mixer_ = std::make_unique<atom::AudioMixer>();
                preview_player_ = std::make_unique<atom::MusicPlayer>(*preview_mixer_);
            }
            const auto path = fs::AssetPath::Parse("preview://" + selected_);
            if (path && preview_player_->Load("preview", *preview_.audio, *path))
                preview_player_->Play("preview", atom::numeric::max_volume);
            else
                preview_.status = "Audio playback unavailable";
        }, true);
        Button({bounds.x + 95, y, 79, 30}, "Stop", [this] {
            if (preview_player_)
                preview_player_->Stop("preview");
        });
    }
}

auto PackagerScreen::DrawBrowse(float bottom) -> void {
    TextField({20, 88, width_ - 311, 36}, Field::Archive, "Package path (.apkg)");
    Button({width_ - 281, 88, 73, 36}, "Choose", [this] { Pick(Field::Archive, platform::PathDialog::OpenFile); });
    Button({width_ - 199, 88, 77, 36}, "Open", [this] { Open(archive_input_); }, !Busy());
    Button({width_ - 113, 88, 93, 36}, "Verify", [this] { Open(archive_, true); });

    constexpr float left = 220, right = 288, top = 140, row = 30;
    const float middle_left = left + 31;
    const float right_left = width_ - right - 20;
    const float middle = right_left - middle_left - 11;
    tree_rect_ = {20, top, left, bottom - top};
    list_rect_ = {middle_left, top, middle, bottom - top};
    const Rect inspector{right_left, top, right, bottom - top};
    for (const auto r : {tree_rect_, list_rect_, inspector}) {
        Fill(r, panel);
        renderer_.DrawRectOutline(r, border);
    }

    Label("DIRECTORIES", 34, top + 11, 13, accent, left - 22);
    const auto tree_row = [&](Rect box, float indent, std::string_view name, bool active,
                              bool expandable, bool open, std::function<void()> select,
                              std::function<void()> toggle) {
        if (active || Contains(box, cursor_x_, cursor_y_))
            Fill(box, active ? selection : field);
        const float x = box.x + indent + 8, y = box.y + 13;
        if (expandable) {
            if (open) {
                renderer_.DrawLine(x - 4, y - 2, x, y + 2, accent, 1.5f);
                renderer_.DrawLine(x, y + 2, x + 4, y - 2, accent, 1.5f);
            } else {
                renderer_.DrawLine(x - 2, y - 4, x + 2, y, muted, 1.5f);
                renderer_.DrawLine(x + 2, y, x - 2, y + 4, muted, 1.5f);
            }
            hits_.push_back({{x - 8, box.y, 18, box.height}, std::move(toggle)});
        }
        Fill({x + 15, y - 6, 6, 3}, accent);
        Fill({x + 14, y - 3, 15, 9}, active ? accent : muted);
        Label(name, x + 35, box.y + 5, 14, ink, box.width - indent - 47);
        hits_.push_back({box, std::move(select)});
    };

    const auto root_name = archive_.empty()
        ? std::string{"Package root"} : atom::PathToUtf8(atom::PathFromUtf8(archive_).filename());
    tree_row({28, top + 35, left - 16, row}, 0, root_name, folder_.empty(),
             !directories_.empty(), root_expanded_,
             [this] { BrowseFolder({}); },
             [this] { root_expanded_ = !root_expanded_; tree_scroll_ = 0; });

    std::vector<std::string_view> shown;
    if (root_expanded_) {
        for (const auto& dir : directories_) {
            const auto parent = pf::PackagePathParent(dir);
            if (parent.empty() || expanded_.contains(parent))
                shown.push_back(dir);
        }
    }
    const auto tree_rows = std::max(0, static_cast<int>((bottom - top - 73) / row));
    tree_scroll_ = std::clamp(tree_scroll_, 0, std::max(0, static_cast<int>(shown.size()) - tree_rows));
    for (int index = tree_scroll_; index < static_cast<int>(shown.size()) && index < tree_scroll_ + tree_rows; ++index) {
        const auto dir = shown[static_cast<std::size_t>(index)];
        const auto depth = static_cast<float>(std::ranges::count(dir, '/'));
        const auto indent = std::min((depth + 1) * 15, 76.0f);
        const auto y = top + 69 + (index - tree_scroll_) * row;
        const auto prefix = std::string{dir} + "/";
        const auto next = std::ranges::lower_bound(directories_, prefix);
        const bool has_children = next != directories_.end() && next->starts_with(prefix);
        tree_row({28, y, left - 16, row}, indent, pf::PackagePathName(dir), dir == folder_,
                 has_children, expanded_.contains(dir),
                 [this, path = std::string{dir}] { BrowseFolder(path); },
                 [this, path = std::string{dir}, prefix] {
                     if (!expanded_.erase(path)) {
                         expanded_.insert(path);
                         return;
                     }
                     auto child = expanded_.lower_bound(prefix);
                     while (child != expanded_.end() && child->starts_with(prefix))
                         child = expanded_.erase(child);
                 });
    }

    TextField({middle_left + 11, top + 11, middle - 158, 34}, Field::Search, "Search in this folder");
    const auto kind_name = kind_ < 0 ? "All types" : pf::KindName(static_cast<std::uint16_t>(kind_));
    Button({right_left - 145, top + 11, 124, 34}, kind_name, [this] {
        kind_ = kind_ == pf::asset_kind::video ? -1 : kind_ + 1;
        Refilter();
    });

    const auto list_y = top + 73;
    const auto name_width = middle - 195;
    const float type_x = middle_left + middle - 172;
    const float size_x = middle_left + middle - 72;
    Label(folder_.empty() ? "/" : folder_, middle_left + 15, top + 52, 12, muted, name_width);
    Label("TYPE", type_x, top + 52, 12, muted, 89);
    Label("SIZE", size_x, top + 52, 12, muted, 63);
    Fill({middle_left + 1, list_y - 2, middle - 2, 1}, border);
    const int rows = std::max(0, static_cast<int>((bottom - list_y - 5) / row));
    const int parent_rows = folder_.empty() ? 0 : 1;
    const int folder_rows = static_cast<int>(visible_directories_.size());
    const int row_count = parent_rows + folder_rows + static_cast<int>(visible_.size());
    list_scroll_ = std::clamp(list_scroll_, 0, std::max(0, row_count - rows));
    if (row_count == 0)
        Label(archive_.empty() ? "Choose a package to see its resources." : "No entries match these filters.",
              middle_left + 15, list_y + 14, 15, muted, middle - 30);
    for (int i = list_scroll_; i < row_count && i < list_scroll_ + rows; ++i) {
        const auto y = list_y + (i - list_scroll_) * row;
        const Rect cell{middle_left + 2, y, middle - 4, row - 1};
        if (i < parent_rows + folder_rows) {
            const bool parent = i < parent_rows;
            const auto path = parent ? pf::PackagePathParent(folder_)
                                     : visible_directories_[static_cast<std::size_t>(i - parent_rows)];
            if (Contains(cell, cursor_x_, cursor_y_))
                Fill(cell, selection);
            else if (i % 2)
                Fill(cell, field);
            Fill({middle_left + 16, y + 8, 6, 3}, accent);
            Fill({middle_left + 15, y + 11, 14, 9}, accent);
            Label(parent ? ".." : pf::PackagePathName(path), middle_left + 38, y + 5, 14, ink, name_width - 23);
            Label(parent ? "Parent" : "Folder", type_x, y + 5, 13, accent, 90);
            hits_.push_back({cell, [this, path = std::string{path}] { BrowseFolder(path); }});
            continue;
        }
        const auto asset_index = visible_[static_cast<std::size_t>(i - parent_rows - folder_rows)];
        const auto& asset = assets_[asset_index];
        if (asset.path == selected_ || Contains(cell, cursor_x_, cursor_y_))
            Fill(cell, selection);
        else if (i % 2)
            Fill(cell, field);
        Label(pf::PackagePathName(asset.path), middle_left + 15, y + 5, 14, ink, name_width);
        Label(pf::KindName(asset.kind), type_x, y + 5, 13, accent, 90);
        Label(SizeLabel(asset.size), size_x, y + 5, 12, muted, 65);
        hits_.push_back({cell, [this, asset_index] {
            if (asset_index < assets_.size())
                SelectAsset(&assets_[asset_index]);
        }});
    }

    const auto x = inspector.x + 15;
    const auto text_width = inspector.width - 30;
    Label("INSPECTOR", x, top + 11, 13, accent, text_width);
    const auto asset = std::ranges::lower_bound(assets_, selected_, {}, &fs::PackagedAssetInfo::path);
    if (selected_.empty() || asset == assets_.end() || asset->path != selected_) {
        Label(archive_.empty() ? "No archive open" : std::to_string(folder_rows) + " folders / " +
              std::to_string(visible_.size()) + " files", x, top + 47, 16, ink, text_width);
        Label("Select a resource for details.", x, top + 79, 13, muted, text_width);
        return;
    }
    Label(pf::PackagePathName(asset->path), x, top + 37, 17, ink, text_width);
    Label(asset->path, x, top + 64, 13, muted, text_width);
    Label(std::string{pf::KindName(asset->kind)} + "  /  " + SizeLabel(asset->size),
          x, top + 86, 13, ink, text_width);
    std::ostringstream hash;
    hash << "FNV-1a  " << std::hex << std::setw(16) << std::setfill('0') << asset->content_hash;
    Label(hash.str(), x, top + 108, 12, muted, text_width);
    DrawPreview({x, top + 139, text_width, bottom - top - 153});
}

auto PackagerScreen::DrawBuild(float bottom) -> void {
    const float card_width = std::min(width_ - 40, 940.0f);
    const float x = (width_ - card_width) * 0.5f;
    Fill({x, 93, card_width, bottom - 93}, panel);
    renderer_.DrawRectOutline({x, 93, card_width, bottom - 93}, border);
    Label("Build an archive", x + 24, 111, 25, ink, card_width - 48);
    Label("Pack one file or recursively include a directory.", x + 25, 148, 14, muted, card_width - 50);
    Label("SOURCE", x + 25, 184, 12, accent, card_width - 50);
    TextField({x + 24, 204, card_width - 220, 34}, Field::Source, "Source path");
    Button({x + card_width - 186, 204, 73, 34}, "File", [this] {
        Pick(Field::Source, platform::PathDialog::OpenFile);
    });
    Button({x + card_width - 104, 204, 80, 34}, "Folder", [this] {
        Pick(Field::Source, platform::PathDialog::OpenFolder);
    });
    Label("OUTPUT", x + 25, 252, 12, accent, card_width - 50);
    TextField({x + 24, 272, card_width - 134, 34}, Field::Output, "Output .apkg");
    Button({x + card_width - 100, 272, 76, 34}, "Choose", [this] {
        Pick(Field::Output, platform::PathDialog::SaveFile);
    });
    Label("PACKAGE PATH", x + 25, 320, 12, accent, card_width - 50);
    TextField({x + 24, 340, card_width - 48, 34}, Field::Prefix,
              "Required for a single file. Optional prefix for a directory.");
    Button({x + 24, 389, 225, 35}, overwrite_ ? "[x] Replace existing archive" : "[ ] Replace existing archive",
           [this] { overwrite_ = !overwrite_; });
    Button({x + card_width - 154, 389, 130, 35}, Busy() ? "Building..." : "Build archive",
           [this] { RequestBuild(); }, !Busy() && !source_.empty() && !output_.empty());
}

auto PackagerScreen::DrawLog(float top) -> void {
    log_rect_ = {20, top, width_ - 40, height_ - top - 34};
    Fill(log_rect_, panel);
    renderer_.DrawRectOutline(log_rect_, border);
    Label("ACTIVITY", 33, top + 7, 12, accent, 150);
    const auto rows = std::max(0, static_cast<int>((log_rect_.height - 27) / 18));
    std::vector<atom::LogRecord> entries;
    {
        const std::scoped_lock lock{logs_->mutex};
        const int count = static_cast<int>(logs_->entries.size());
        log_scroll_ = std::clamp(log_scroll_, 0, std::max(0, count - rows));
        const auto end = logs_->entries.end() - log_scroll_;
        const auto begin = end - std::min(count - log_scroll_, rows);
        entries.assign(begin, end);
    }
    if (entries.empty())
        Label("Package operations will appear here.", 33, top + 30, 13, muted, width_ - 66);
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto& record = entries[i];
        const auto y = top + 26 + static_cast<float>(i) * 18;
        Label(atom::ToString(record.level), 33, y, 12, record.level == atom::LogLevel::ATOM_ERROR ? failure : accent, 72);
        Label(record.message, 107, y, 13, muted, width_ - 140);
    }
}

} // namespace

auto atom::packager::RunGui(const std::vector<std::string>& args) -> int {
    atom::Log::SetConsoleOutputUtf8();
    unsigned frames = 0;
    std::string initial;
    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string_view arg{args[i]};
        if (arg == "--open" && i + 1 < args.size()) {
            initial = args[++i];
        } else if (arg.starts_with("--frames=")) {
            auto value = arg.substr(9);
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), frames);
            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || frames == 0)
                return 2;
        } else {
            return 2;
        }
    }

    auto& app = atom::runtime::Application::GetInstance();
    const auto initialized = app.Initialize({.title = "Atom Packager", .resolution = {1280, 800}});
    if (!initialized) {
        LOG_ERROR(atom::log::utilities::Packager, initialized.error().Message());
        return 1;
    }
    auto& jobs = atom::job::JobSystem::GetInstance();
    jobs.Configure(2);
    jobs.Start();

    auto screen = std::make_unique<PackagerScreen>(app, frames);
    auto* tool = screen.get();
    auto* device = app.GetWindow().GetRenderDevice();
    if (!device || !tool->Initialize(*device)) {
        LOG_ERROR(atom::log::utilities::Packager, "Could not initialize Renderer2D or load a desktop font");
        tool->Shutdown();
        app.Shutdown();
        jobs.Shutdown();
        return 1;
    }
    auto& screens = app.GetScreenManager();
    if (screens.LoadScreen("Packager", std::move(screen)) != atom::ScreenOpResult::Success ||
        screens.SwitchScreen("Packager") != atom::ScreenOpResult::Success) {
        app.Shutdown();
        jobs.Shutdown();
        return 1;
    }
    bool render_failed = false;
    const auto cleanup = app.AddShutdownObserver([&] {
        render_failed = tool->Failed();
        tool->Shutdown();
    });
    tool->Open(std::move(initial));
    app.Run();
    jobs.Shutdown();
    return render_failed ? 1 : 0;
}
