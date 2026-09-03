#include "VoicePool.hpp"
#include <algorithm>
#include <utility>
namespace atom {
VoicePool::VoicePool(atom::audio::IAudioBackend& backend, std::shared_ptr<const atom::audio::DecodedAudio> clip,
                     const std::size_t maximum)
    : backend_(backend), clip_(std::move(clip)), maximum_(std::max<std::size_t>(1, maximum)) {}
auto VoicePool::ReapFinished() -> void {
    for (auto& voice : voices_)
        if (voice && voice->GetState() == atom::audio::AudioSourceState::Playing && voice->IsFinished())
            voice->Stop();
}
auto VoicePool::Acquire() -> atom::audio::IAudioSource* {
    ReapFinished();
    for (auto& voice : voices_)
        if (voice && voice->GetState() == atom::audio::AudioSourceState::Stopped)
            return voice.get();
    if (voices_.size() < maximum_) {
        auto voice = backend_.CreateSFXSource(clip_->pcm, clip_->spec);
        if (!voice)
            return nullptr;
        auto* result = voice.get();
        voices_.push_back(std::move(voice));
        return result;
    }
    auto* result = voices_.front().get();
    result->Stop();
    std::rotate(voices_.begin(), voices_.begin() + 1, voices_.end());
    return result;
}
auto VoicePool::StopAll() -> void {
    for (auto& voice : voices_)
        if (voice)
            voice->Stop();
}
auto VoicePool::SetVolume(const float volume) -> void {
    for (auto& voice : voices_)
        if (voice)
            voice->SetVolume(volume);
}
auto VoicePool::FirstActive() -> atom::audio::IAudioSource* {
    for (auto& voice : voices_)
        if (voice && !voice->IsFinished())
            return voice.get();
    return voices_.empty() ? nullptr : voices_.front().get();
}
} // namespace atom
