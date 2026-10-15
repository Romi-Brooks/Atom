/**
  * @file           : InputDemo.cpp
  * @author         : Romi Brooks
  * @brief          : CORE-002 acceptance example: a box driven by the per-frame
  *                   InputSystem, a game-side chat box that captures input, and
  *                   an overlay that prints the raw state the engine exposes.
  * @attention      : Proves that RenderWindow feeds InputSystem automatically;
  *                   the screen only reads state, it never polls SDL or folds
  *                   events itself. The debug panel is optional - the chat box
  *                   works the same in a build without the debugger.
  * @date           : 2026/10/15
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include <Window/RenderWindow.hpp>
#include <Window/Screen.hpp>
#include <Window/ScreenManager.hpp>

#include <Debugger/Overlay.hpp>
#include <Input/ActionMap.hpp>
#include <Input/InputSystem.hpp>
#include <Log/LogSystem.hpp>
#include <Render/Renderer2D/Renderer2D.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace {

// The application owns its action enum and its bindings; the engine only owns
// Key. Keeping actions semantic means the bindings can be replaced (options
// menu, rebinding screen) without touching the game logic below.
enum class Action { TogglePause, Recenter, Quit, OpenChat, SendChat, CloseChat };

// Keys whose held state drives continuous motion. These are read directly
// instead of through ActionMap because motion is a "held" query, while the
// ActionMap below demonstrates one-shot "pressed" queries.
struct MoveKeys {
        atom::input::Key up = atom::input::Key::W;
        atom::input::Key down = atom::input::Key::S;
        atom::input::Key left = atom::input::Key::A;
        atom::input::Key right = atom::input::Key::D;
};

class InputDemoScreen final : public atom::Screen {
    public:
        InputDemoScreen() {
            // Optional alternative bindings: the same action can answer to
            // several chords, so the arrow keys work alongside WASD.
            actions_.Bind({atom::input::Key::Space}, Action::TogglePause);
            actions_.Bind({atom::input::Key::R}, Action::Recenter);
            actions_.Bind({atom::input::Key::Escape}, Action::Quit);
            actions_.Bind({atom::input::Key::T}, Action::OpenChat);
            actions_.Bind({atom::input::Key::Enter}, Action::SendChat);
        }

        ~InputDemoScreen() override {
            // The GPU device outlives the screen (RenderWindow owns it), so the
            // renderer must release its resources before that.
            renderer_.Shutdown();
        }

        auto Render(atom::render::IRenderDevice& device) -> void override {
            if (!renderer_.IsInitialized() && !renderer_.Initialize(device, ATOM_SHADER_OUTPUT_DIR)) {
                if (!render_failed_) {
                    LOG_ERROR(atom::log::core::Screen, "Input demo: Renderer2D initialization failed");
                    render_failed_ = true;
                }
                return;
            }

            const auto size = device.GetOutputSize();
            window_width_ = size.GetX();
            window_height_ = size.GetY();

            device.Clear(atom::render::Color{18, 20, 28, 255});

            if (!renderer_.BeginFrame(0.0f, 0.0f, 1.0f))
                return;

            // A pulsing ring under the box: it only moves when Update() ran, so
            // a frozen pulse means the update path stopped feeding input.
            const float pulse = 6.0f + 3.0f * std::sin(pulse_time_ * 3.0f);
            renderer_.DrawRect(atom::algo::Rect{player_.x - pulse, player_.y - pulse,
                                                player_.width + pulse * 2.0f, player_.height + pulse * 2.0f},
                               atom::render::Color{48, 62, 96, 160});

            const auto body = paused_ ? atom::render::Color{120, 120, 130, 255}
                                      : atom::render::Color{92, 168, 255, 255};
            renderer_.DrawRect(player_, body);

            if (paused_) {
                // A cross over the box makes the paused action state visible
                // without needing a font.
                renderer_.DrawLine(player_.x, player_.y, player_.x + player_.width, player_.y + player_.height,
                                   atom::render::Color{255, 90, 90, 255}, 3.0f);
                renderer_.DrawLine(player_.x + player_.width, player_.y, player_.x, player_.y + player_.height,
                                   atom::render::Color{255, 90, 90, 255}, 3.0f);
            }

            DrawChat(renderer_);

            renderer_.EndFrame();
        }

        // Held-state queries live here: the engine settled the previous frame's
        // edges in BeginFrame(), so these are steady-state reads.
        auto FixedUpdate(float delta_time) -> void override {
            const auto& input = atom::input::InputSystem::GetInstance();

            // Movement is continuous input, so it answers to the narrow gate: it
            // stops only while a text field is being typed into. Merely having a
            // debug panel focused must not freeze the box.
            const bool typing = input.IsTextInputCapturedByUi();

            float direction_x = 0.0f;
            float direction_y = 0.0f;
            if (!typing) {
                if (input.IsKeyHeld(keys_.left) || input.IsKeyHeld(atom::input::Key::Left))
                    direction_x -= 1.0f;
                if (input.IsKeyHeld(keys_.right) || input.IsKeyHeld(atom::input::Key::Right))
                    direction_x += 1.0f;
                if (input.IsKeyHeld(keys_.up) || input.IsKeyHeld(atom::input::Key::Up))
                    direction_y -= 1.0f;
                if (input.IsKeyHeld(keys_.down) || input.IsKeyHeld(atom::input::Key::Down))
                    direction_y += 1.0f;
            }

            // Normalize so diagonal movement is not faster than axis movement.
            const float length = std::sqrt(direction_x * direction_x + direction_y * direction_y);
            if (length > 0.0f) {
                direction_x /= length;
                direction_y /= length;
            }

            if (paused_)
                return;

            player_.x += direction_x * speed_ * delta_time;
            player_.y += direction_y * speed_ * delta_time;

            // Keyboard focus can take the cursor away, so the box is also
            // draggable; both paths go through the same position.
            //
            // While the UI owns the mouse, the raw state below still reports
            // "left button held with the cursor at x/y" - it is the same
            // physical mouse that is dragging the panel. This gate is what
            // keeps a panel drag from dragging the box as well.
            if (input.IsMouseCapturedByUi()) {
                dragging_ = false;
            } else if (input.IsMouseHeld(atom::input::MouseButton::Left)) {
                const auto position = input.GetMousePosition();
                if (dragging_) {
                    player_.x += position.GetX() - drag_origin_.GetX();
                    player_.y += position.GetY() - drag_origin_.GetY();
                }
                drag_origin_ = position;
                dragging_ = true;
            } else {
                dragging_ = false;
            }

            ClampToWindow();
        }

        // Variable-rate update: one-shot action edges, chat ownership and
        // presentation state.
        auto Update(const float delta_time) -> void override {
            pulse_time_ += std::max(0.0f, delta_time);
            auto& input = atom::input::InputSystem::GetInstance();

            // Claim the keyboard *after* applying this frame's chat command, not
            // before. Publishing the claim first advertised "the game owns the
            // keyboard" for the very frame that opens the chat, so typing began
            // one frame before movement was suppressed - and while that stale
            // claim was still up, the T that opened the chat was re-read as the
            // chat key itself. Order matters: decide, then publish.
            const bool opening_chat =
                !chat_open_ && !input.IsKeyboardCapturedByUi() && input.IsActionPressed(actions_, Action::OpenChat);
            if (opening_chat) {
                chat_open_ = true;
                chat_draft_.clear();
                LOG_INFO(atom::log::core::Screen, "Input demo: chat opened (typing captures movement)");
            }
            input.SetUiCapture(input.IsMouseCapturedByUi(), chat_open_, chat_open_, "chat box");

            if (input.IsKeyboardCapturedByUi()) {
                HandleChatKeys();
                return;
            }

            if (input.IsActionPressed(actions_, Action::Quit)) {
                LOG_INFO(atom::log::core::Screen, "Input demo: quit action (Escape)");
                atom::RenderWindow::GetInstance().Shutdown();
                return;
            }
            if (input.IsActionPressed(actions_, Action::TogglePause)) {
                paused_ = !paused_;
                LOG_INFO(atom::log::core::Screen, std::string{"Input demo: "} + (paused_ ? "paused" : "resumed"));
            }
            if (input.IsActionPressed(actions_, Action::Recenter))
                CenterPlayer();
        }

        // Text arrives as its own event stream, not as key presses: that is what
        // makes a chat box work with any keyboard layout.
        auto HandleEvent(const atom::window::IEvent& event) -> bool override {
            if (event.type != atom::window::EventType::TextInput || !chat_open_)
                return false;
            const auto& text = std::get<atom::window::TextInputEvent>(event.data);
            if (chat_draft_.size() + text.text.size() > 48)
                return true;
            chat_draft_ += text.text;
            return true;
        }

        [[nodiscard]] auto IsPaused() const -> bool {
            return paused_;
        }

        [[nodiscard]] auto PlayerRect() const -> const atom::algo::Rect& {
            return player_;
        }

        [[nodiscard]] auto IsChatOpen() const -> bool {
            return chat_open_;
        }

        [[nodiscard]] auto ChatDraft() const -> const std::string& {
            return chat_draft_;
        }

        [[nodiscard]] auto LastMessage() const -> const std::string& {
            return last_message_;
        }

    private:
        // Chat commands run on the key *event* path, which is the right tool
        // here: Enter and Backspace are discrete commands, not held states.
        auto HandleChatKeys() -> void {
            auto& input = atom::input::InputSystem::GetInstance();
            if (input.IsKeyPressed(atom::input::Key::Backspace) && !chat_draft_.empty())
                chat_draft_.pop_back();

            if (input.IsKeyPressed(atom::input::Key::Escape)) {
                chat_open_ = false;
                chat_draft_.clear();
                return;
            }

            if (input.IsKeyPressed(atom::input::Key::Enter)) {
                if (!chat_draft_.empty()) {
                    last_message_ = chat_draft_;
                    LOG_INFO(atom::log::core::Screen, "Input demo: chat sent \"" + chat_draft_ + "\"");
                }
                chat_draft_.clear();
                chat_open_ = false;
            }
        }

        // The chat has no text renderer behind it in this example, so it is
        // drawn with primitives: the field turns amber while it owns the
        // keyboard, a caret blinks, and the sent message shows as a bar above
        // it. The point is the input contract, not the widget.
        auto DrawChat(atom::render::Renderer2D& renderer) -> void {
            const float field_height = 34.0f;
            const float margin = 24.0f;
            const auto field = atom::algo::Rect{margin, window_height_ - field_height - margin,
                                                std::max(120.0f, window_width_ - margin * 2.0f), field_height};

            const auto border = chat_open_ ? atom::render::Color{255, 196, 92, 255}
                                           : atom::render::Color{64, 70, 88, 255};
            renderer.DrawRect(atom::algo::Rect{field.x - 1.0f, field.y - 1.0f, field.width + 2.0f,
                                               field.height + 2.0f},
                              border);
            renderer.DrawRect(field, atom::render::Color{26, 29, 40, 235});

            if (chat_open_) {
                // One block per typed character, capped so the row stays inside
                // the field. The caret blinks at 2 Hz.
                const float cell = 12.0f;
                const std::size_t shown = std::min<std::size_t>(chat_draft_.size(), 40);
                for (std::size_t index = 0; index < shown; ++index) {
                    renderer.DrawRect(atom::algo::Rect{field.x + 10.0f + static_cast<float>(index) * (cell + 2.0f),
                                                       field.y + 11.0f, cell, 12.0f},
                                      atom::render::Color{198, 214, 246, 255});
                }
                if (std::fmod(pulse_time_, 1.0f) < 0.5f) {
                    renderer.DrawRect(atom::algo::Rect{field.x + 10.0f + static_cast<float>(shown) * (cell + 2.0f),
                                                       field.y + 8.0f, 2.0f, 18.0f},
                                      atom::render::Color{255, 196, 92, 255});
                }
            }

            if (!last_message_.empty()) {
                const float width = std::min(360.0f, 20.0f + static_cast<float>(last_message_.size()) * 8.0f);
                renderer_.DrawRect(atom::algo::Rect{field.x, field.y - 30.0f, width, 20.0f},
                                   atom::render::Color{92, 168, 255, 200});
            }
        }

        auto CenterPlayer() -> void {
            player_.x = (window_width_ - player_.width) * 0.5f;
            player_.y = (window_height_ - player_.height) * 0.5f;
        }

        auto ClampToWindow() -> void {
            auto* device = atom::RenderWindow::GetInstance().GetRenderDevice();
            if (device == nullptr)
                return;
            const auto output = device->GetOutputSize();
            window_width_ = output.GetX();
            window_height_ = output.GetY();
            player_.x = std::clamp(player_.x, 0.0f, std::max(0.0f, window_width_ - player_.width));
            player_.y = std::clamp(player_.y, 0.0f, std::max(0.0f, window_height_ - player_.height));
        }

        atom::render::Renderer2D renderer_{};
        atom::input::ActionMap<Action> actions_{};
        MoveKeys keys_{};
        atom::algo::Rect player_{0.0f, 0.0f, 96.0f, 96.0f};
        atom::algo::Vec2 drag_origin_{};
        std::string chat_draft_{};
        std::string last_message_{};
        float speed_ = 420.0f;
        float pulse_time_ = 0.0f;
        float window_width_ = 1280.0f;
        float window_height_ = 720.0f;
        bool dragging_ = false;
        bool paused_ = false;
        bool chat_open_ = false;
        bool render_failed_ = false;
};

// Optional debug panel. It only reads engine state; the demo's behaviour does
// not depend on it being present.
class InputStatePanel final : public atom::debugger::DebugPanel {
    public:
        InputStatePanel() : DebugPanel("InputState") {}

    protected:
        auto OnDrawOverlay() -> void override {
            const auto& input = atom::input::InputSystem::GetInstance();
            const auto mouse = input.GetMousePosition();
            const auto delta = input.GetMouseDelta();
            const auto wheel = input.GetMouseWheel();

            ImGui::SetNextWindowSize(ImVec2{360.0f, 0.0f}, ImGuiCond_FirstUseEver);
            ImGui::Begin("Input state");

            ImGui::TextUnformatted("Held keys");
            ImGui::Separator();
            const auto key_row = [&input](const char* label, const atom::input::Key key) {
                const auto state = input.GetKeyState(key);
                const char* name = "Up";
                switch (state) {
                    case atom::input::KeyState::Up:
                        name = "Up";
                        break;
                    case atom::input::KeyState::Pressed:
                        name = "Pressed";
                        break;
                    case atom::input::KeyState::Held:
                        name = "Held";
                        break;
                    case atom::input::KeyState::Released:
                        name = "Released";
                        break;
                }
                ImGui::Text("%-10s %s", label, name);
            };
            key_row("W", atom::input::Key::W);
            key_row("A", atom::input::Key::A);
            key_row("S", atom::input::Key::S);
            key_row("D", atom::input::Key::D);
            key_row("Space", atom::input::Key::Space);
            key_row("LShift", atom::input::Key::LeftShift);
            key_row("LCtrl", atom::input::Key::LeftControl);

            ImGui::Separator();
            ImGui::TextUnformatted("Mouse");
            ImGui::Text("position  %7.1f, %7.1f", mouse.GetX(), mouse.GetY());
            ImGui::Text("delta     %7.1f, %7.1f", delta.GetX(), delta.GetY());
            ImGui::Text("wheel     %7.1f, %7.1f", wheel.GetX(), wheel.GetY());
            ImGui::Text("left      %s", input.IsMouseHeld(atom::input::MouseButton::Left) ? "held" : "up");
            ImGui::Text("right     %s", input.IsMouseHeld(atom::input::MouseButton::Right) ? "held" : "up");

            ImGui::Separator();
            ImGui::TextUnformatted("Chords and actions");
            ImGui::Text("Ctrl+S pressed : %s", input.IsChordPressed({atom::input::Key::S,
                                                                     atom::input::KeyModifiers::Control})
                                                   ? "yes"
                                                   : "no");
            ImGui::Text("Shift+Space    : %s", input.IsChordHeld({atom::input::Key::Space,
                                                                  atom::input::KeyModifiers::Shift})
                                                   ? "yes"
                                                   : "no");

            ImGui::Separator();
            ImGui::TextUnformatted("UI capture (who owns the keyboard now)");
            const auto source = input.UiCaptureSource();
            // %.*s takes the length explicitly, so a string_view can be printed
            // without materializing a temporary std::string.
            ImGui::Text("source   %.*s", static_cast<int>(source.size()), source.data());
            ImGui::Text("mouse    %s", input.IsMouseCapturedByUi() ? "captured" : "free");
            ImGui::Text("keyboard %s", input.IsKeyboardCapturedByUi() ? "captured" : "free");
            ImGui::Text("text     %s", input.IsTextInputCapturedByUi() ? "captured" : "free");
            ImGui::TextDisabled("mouse: a panel is being dragged");
            ImGui::TextDisabled("keyboard: the game's chat box is open");
            ImGui::TextDisabled("text: the chat box is typing (stops WASD)");

            const auto* screen = dynamic_cast<const InputDemoScreen*>(
                atom::ScreenManager::GetInstance().GetScreen("InputDemo"));
            if (screen != nullptr) {
                const auto player = screen->PlayerRect();
                ImGui::Separator();
                ImGui::Text("box       %6.1f, %6.1f", player.x, player.y);
                ImGui::Text("paused    %s", screen->IsPaused() ? "yes" : "no");
                ImGui::Text("chat      %s", screen->IsChatOpen() ? "open" : "closed");
                ImGui::Text("draft     %s",
                            screen->ChatDraft().empty() ? "(empty)" : screen->ChatDraft().c_str());
                ImGui::Text("sent      %s",
                            screen->LastMessage().empty() ? "(none)" : screen->LastMessage().c_str());
            }

            ImGui::Separator();
            ImGui::TextUnformatted("WASD / arrows : move the box");
            ImGui::TextUnformatted("drag          : move the box with the mouse");
            ImGui::TextUnformatted("T             : open the chat (game UI takes input)");
            ImGui::TextUnformatted("type / Enter  : type a message, Enter sends");
            ImGui::TextUnformatted("Space         : pause / resume");
            ImGui::TextUnformatted("R             : recenter");
            ImGui::TextUnformatted("Escape        : close chat, or quit");
            ImGui::TextDisabled("While the chat is open, R and Space do nothing:");
            ImGui::TextDisabled("the game UI owns the keyboard.");

            ImGui::End();
        }
};

} // namespace

auto main() -> int {
    // Windows console: make the debugger's UTF-8 output readable.
    atom::Log::SetConsoleOutputUtf8();
    atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_INFO);

    auto& screens = atom::ScreenManager::GetInstance();
    const auto loaded = screens.LoadScreen("InputDemo", std::make_unique<InputDemoScreen>());
    if (loaded != atom::ScreenOpResult::Success) {
        LOG_ERROR(atom::log::core::ScreenManager,
                  "Failed to register the input demo screen: " + std::string{atom::ToString(loaded)});
        return 1;
    }
    if (screens.SwitchScreen("InputDemo") != atom::ScreenOpResult::Success) {
        LOG_ERROR(atom::log::core::ScreenManager, "Failed to switch to the input demo screen");
        return 1;
    }

    auto& window = atom::RenderWindow::GetInstance();
    const auto initialized = window.Initialize("Atom Engine - Input System Demo", atom::algo::Vec2{1280, 720},
                                              atom::backend::RenderBackendId::SdlGpu);
    if (!initialized) {
        LOG_ERROR(atom::log::core::Main, "Window initialization failed: " + initialized.error().Message());
        return 1;
    }
    window.SetFPS(60);

    InputStatePanel panel{};
    panel.Attach(window);

    LOG_INFO(atom::log::core::Screen,
             "Input demo running. WASD/arrows move the box, T opens a chat box, Space pauses, R recenters, "
             "Escape quits.");

    window.Run();
    return 0;
}
