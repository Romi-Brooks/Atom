#ifndef ATOM_ITEXTURE_HPP
#define ATOM_ITEXTURE_HPP

#include <cstdint>
#include <string>

#include <Algorithm/Vector/Vec2.hpp>

namespace atom::render {

class ITexture {
    public:
        virtual ~ITexture() = default;

        virtual auto LoadFromFile(const std::string& path) -> bool = 0;
        virtual auto LoadFromMemory(const uint8_t* data, uint32_t width, uint32_t height) -> bool = 0;
        virtual auto Update(const uint8_t* pixels, uint32_t width, uint32_t height) -> bool = 0;
        [[nodiscard]] virtual auto GetSize() const -> algo::Vec2 = 0;
};

} // namespace atom::render

#endif // ATOM_ITEXTURE_HPP
