#include "SDL3DebugImGuiBackend.hpp"

#include <Backend/Contracts/Debug/IDebugImGuiBackend.hpp>
#include <Backend/Contracts/Render/IRenderWindow.hpp>
#include <Backend/SDL3/Window/ISDL3WindowExtensions.hpp>

// Third Party
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

namespace atom {
namespace {

// ImGui platform/render glue for the SDL3 render backend. All SDL3/ImGui
// coupling lives here; the public Debugger and the engine headers stay
// backend-free.
class SDL3DebugImGuiBackend final : public IDebugImGuiBackend {
public:
    SDL3DebugImGuiBackend(SDL_Window* window, SDL_Renderer* renderer) : window_(window), renderer_(renderer) {}

    auto Initialize() -> bool override {
        if (!window_ || !renderer_)
            return false;
        ImGui::CreateContext();
        ImGui_ImplSDL3_InitForSDLRenderer(window_, renderer_);
        ImGui_ImplSDLRenderer3_Init(renderer_);
        return true;
    }

    auto NewFrame() -> void override {
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
    }

    auto Render() -> void override {
        ImGui::Render();
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer_);
    }

    auto ProcessRawEvent(const void* rawEvent) -> void override {
        ImGui_ImplSDL3_ProcessEvent(static_cast<const SDL_Event*>(rawEvent));
    }

    auto Shutdown() -> void override {
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
    }

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
};

} // namespace

auto CreateDebugImGuiBackend(IRenderWindow& window) -> std::unique_ptr<IDebugImGuiBackend> {
    auto* extensions = dynamic_cast<ISDL3WindowExtensions*>(&window);
    if (!extensions)
        return nullptr;
    return std::make_unique<SDL3DebugImGuiBackend>(extensions->GetNativeWindow(), extensions->GetNativeRenderer());
}

} // namespace atom
