#ifndef ATOM_SDL3_WINDOW_EXTENSIONS_HPP
#define ATOM_SDL3_WINDOW_EXTENSIONS_HPP

struct SDL_Window;

namespace atom::backend::sdl3 {

// Backend-specific extension interface (RENDER-001): native handles exist only
// on backend-specific extension interfaces, never on the generic IWindow.
// Query with dynamic_cast from engine-internal platform adapters only (e.g.
// the ImGui debug backend); game code must not use this.
class ISDL3WindowExtensions {
    public:
        virtual ~ISDL3WindowExtensions() = default;

        [[nodiscard]] virtual auto GetNativeWindow() const -> SDL_Window* = 0;
};

} // namespace atom::backend::sdl3

#endif // ATOM_SDL3_WINDOW_EXTENSIONS_HPP
