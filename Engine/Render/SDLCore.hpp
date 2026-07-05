#ifndef ATOM_SDL3_CORE_HPP
#define ATOM_SDL3_CORE_HPP

#include <atomic>
#include <cstdint>

namespace atom {

class SDLCore {
public:
    struct Config {
        bool video;
        bool audio;
        bool events;

        Config() : video(true), audio(true), events(true) {}
    };

    SDLCore() = delete;

    static auto Initialize(const Config& cfg = Config{}) -> bool;
    static auto Shutdown() -> void;
    [[nodiscard]] static auto RefCount() -> uint32_t;

private:
    static std::atomic<uint32_t> ref_count_;
    static Config active_config_;
};

} // namespace atom

#endif // ATOM_SDL3_CORE_HPP
