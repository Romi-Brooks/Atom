#ifndef ATOM_SDL3_TEXTURE_HPP
#define ATOM_SDL3_TEXTURE_HPP

#include <SDL3/SDL.h>
#include <Engine/Interfaces/ITexture.hpp>

namespace atom {

class SDL3Texture : public ITexture {
public:
    explicit SDL3Texture(SDL_Renderer* renderer);
    ~SDL3Texture() override;

    SDL3Texture(const SDL3Texture&) = delete;
    auto operator=(const SDL3Texture&) -> SDL3Texture& = delete;

    auto LoadFromFile(const std::string& path) -> bool override;
    auto LoadFromMemory(const uint8_t* data, uint32_t width, uint32_t height) -> bool override;
    auto Update(const uint8_t* pixels, uint32_t width, uint32_t height) -> bool override;
    [[nodiscard]] auto GetSize() const -> Vec2 override;

    [[nodiscard]] auto GetNativeTexture() const -> SDL_Texture* { return texture_; }

private:
    SDL_Texture* texture_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    Vec2 size_;
};

} // namespace atom

#endif // ATOM_SDL3_TEXTURE_HPP
