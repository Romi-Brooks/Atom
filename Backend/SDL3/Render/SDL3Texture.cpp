#include "SDL3Texture.hpp"

#include <SDL3/SDL.h>

namespace atom {

SDL3Texture::SDL3Texture(SDL_Renderer* renderer)
    : renderer_(renderer) {}

SDL3Texture::~SDL3Texture() {
    if (texture_) SDL_DestroyTexture(texture_);
}

auto SDL3Texture::LoadFromFile(const std::string& path) -> bool {
    SDL_Surface* surface = SDL_LoadPNG(path.c_str());
    if (!surface) {
        return false;
    }

    if (texture_) SDL_DestroyTexture(texture_);
    texture_ = SDL_CreateTextureFromSurface(renderer_, surface);
    if (texture_) {
        size_ = {static_cast<float>(surface->w), static_cast<float>(surface->h)};
    }
    SDL_DestroySurface(surface);
    return texture_ != nullptr;
}

auto SDL3Texture::LoadFromMemory(const uint8_t* data, uint32_t width, uint32_t height) -> bool {
    if (texture_) SDL_DestroyTexture(texture_);

    texture_ = SDL_CreateTexture(renderer_,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STATIC,
        static_cast<int>(width),
        static_cast<int>(height));
    if (!texture_) return false;

    size_ = {static_cast<float>(width), static_cast<float>(height)};
    return Update(data, width, height);
}

auto SDL3Texture::Update(const uint8_t* pixels, uint32_t width, uint32_t height) -> bool {
    if (!texture_) return false;
    const int pitch = static_cast<int>(width * 4); // RGBA8888
    return SDL_UpdateTexture(texture_, nullptr, pixels, pitch);
}

auto SDL3Texture::GetSize() const -> Vec2 {
    return size_;
}

} // namespace atom
