#include <memory>

#include <Atom/Debugger/Overlay.hpp>

namespace {

class SdkPanel final : public atom::debugger::DebugPanel {
    public:
        SdkPanel() : DebugPanel("SDK ImGui panel") {}

        auto Draw() -> bool {
            OnDrawOverlay();
            return layout_applied_;
        }

    protected:
        auto OnDrawOverlay() -> void override {
            atom::debugger::ApplyPanelSlot(12.0F, 16.0F, 240.0F, 160.0F);
            if (ImGui::Begin(GetPanelName().c_str())) {
                const auto position = ImGui::GetWindowPos();
                const auto size = ImGui::GetWindowSize();
                layout_applied_ = position.x == 12.0F && position.y == 16.0F && size.x == 240.0F && size.y == 160.0F;
                ImGui::TextUnformatted("Drawn from the SDK consumer");
            }
            ImGui::End();
        }

    private:
        bool layout_applied_ = false;
};

} // namespace

auto main() -> int {
    const std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)> context{ImGui::CreateContext(),
                                                                                  &ImGui::DestroyContext};
    if (!context) {
        return 1;
    }

    // A headless renderer can build the atlas without opening an SDL window.
    auto& io = ImGui::GetIO();
    io.DisplaySize = ImVec2{640.0F, 480.0F};
    io.IniFilename = nullptr;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    if (!pixels || width <= 0 || height <= 0) {
        return 2;
    }
    io.Fonts->SetTexID(ImTextureID{1});

    SdkPanel panel;
    ImGui::NewFrame();
    const bool layout_applied = panel.Draw();
    ImGui::Render();

    // ApplyPanelSlot ran inside Atom. The consumer must see those same ImGui
    // coordinates and draw data from the context it created.
    const auto* draw_data = ImGui::GetDrawData();
    return layout_applied && draw_data && draw_data->TotalVtxCount > 0 ? 0 : 3;
}
