#include <Backend/Contracts/Audio/IAudioDecoder.hpp>
#include <Backend/Extension/AudioDecoderRegistry.hpp>
#include <Test/Support/TestHelpers.hpp>

#include <memory>
#include <string>

namespace {

class StubDecoder final : public atom::audio::IAudioDecoder {
    public:
        explicit StubDecoder(std::string marker) : marker_(std::move(marker)) {}

        [[nodiscard]] auto Open(const std::string&) -> atom::audio::DecoderOpenStatus override {
            return atom::audio::DecoderOpenStatus::UnsupportedFormat;
        }
        [[nodiscard]] auto OpenFromMemory(const void*, std::size_t) -> atom::audio::DecoderOpenStatus override {
            return atom::audio::DecoderOpenStatus::UnsupportedFormat;
        }
        auto Close() -> void override {}
        auto DecodeChunk(uint8_t*, uint32_t) -> uint32_t override { return 0; }
        auto Rewind() -> bool override { return true; }
        [[nodiscard]] auto GetInfo() const -> const atom::audio::DecoderInfo& override { return info_; }
        [[nodiscard]] auto IsOpen() const -> bool override { return false; }
        [[nodiscard]] auto Marker() const -> const std::string& { return marker_; }

    private:
        std::string marker_{};
        atom::audio::DecoderInfo info_{};
};

} // namespace

auto main() -> int {
    using namespace atom::audio;

    AudioDecoderRegistry registry{};
    ATOM_CHECK(!registry.Contains("wav"));
    ATOM_CHECK(!registry.Contains(".wav"));

    ATOM_CHECK(registry.Register("wav", [] { return std::make_unique<StubDecoder>("primary"); }, "primary"));
    ATOM_CHECK(registry.Contains("wav"));
    ATOM_CHECK(registry.Contains(".WAV"));
    ATOM_CHECK(!registry.Register("wav", [] { return std::make_unique<StubDecoder>("ignored"); }));

    ATOM_CHECK(registry.RegisterFallback(".wav", [] { return std::make_unique<StubDecoder>("fallback"); }, "fallback"));

    const auto candidates = registry.CandidatesForFile("sounds/shot.WAV");
    ATOM_CHECK(candidates.size() == 2);
    ATOM_CHECK(candidates[0].name == "primary");
    ATOM_CHECK(candidates[1].name == "fallback");
    auto first = candidates[0].factory();
    auto second = candidates[1].factory();
    ATOM_CHECK(first && dynamic_cast<StubDecoder*>(first.get())->Marker() == "primary");
    ATOM_CHECK(second && dynamic_cast<StubDecoder*>(second.get())->Marker() == "fallback");

    auto only = registry.CreateForFile("a.wav");
    ATOM_CHECK(only && dynamic_cast<StubDecoder*>(only.get())->Marker() == "primary");
    ATOM_CHECK(!registry.CreateForFile("a.mp3"));

    ATOM_CHECK(registry.Replace("mp3", [] { return std::make_unique<StubDecoder>("mp3-only"); }, "mp3-only"));
    ATOM_CHECK(registry.Contains("mp3"));
    const auto mp3 = registry.CandidatesForFile("x.MP3");
    ATOM_CHECK(mp3.size() == 1 && mp3[0].name == "mp3-only");

    ATOM_CHECK(registry.Unregister("wav"));
    ATOM_CHECK(!registry.Contains("wav"));
    ATOM_CHECK(!registry.Unregister("wav"));
    return 0;
}
