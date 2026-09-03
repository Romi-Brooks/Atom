#ifndef ATOM_IMEDIA_DECODER_HPP
#define ATOM_IMEDIA_DECODER_HPP

#include <cstdint>
#include <string>

namespace atom::video {

struct MediaFrameInfo {
        uint32_t width = 0, height = 0;
        float frameRate = 0, duration = 0;
};

class IMediaDecoder {
    public:
        virtual ~IMediaDecoder() = default;

        virtual auto Open(const std::string& path) -> bool = 0;
        virtual auto Close() -> void = 0;
        [[nodiscard]] virtual auto GetInfo() const -> MediaFrameInfo = 0;
        virtual auto DecodeNextFrame() -> bool = 0;
        [[nodiscard]] virtual auto GetFrameData() const -> const uint8_t* = 0;
        virtual auto Seek(float seconds) -> bool = 0;
};

} // namespace atom::video

#endif // ATOM_IMEDIA_DECODER_HPP
