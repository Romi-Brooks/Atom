#include "VoicePool.hpp"
#include <algorithm>
#include <utility>
namespace atom {
VoicePool::VoicePool(atom::audio::IAudioBackend& backend, std::shared_ptr<const atom::audio::DecodedAudio> clip,
                     const std::size_t maximum)
    : backend_(backend), clip_(std::move(clip)), maximum_(atom::numeric::SanitizeVoiceCount(maximum)) {}
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
    // One sanitized gain for the whole pool: a NaN would otherwise reach
    // SDL_SetAudioStreamGain through every voice.
    const float gain = atom::numeric::SanitizeVolume(volume);
    for (auto& voice : voices_)
        if (voice)
            voice->SetVolume(gain);
}
auto VoicePool::FirstActive() -> atom::audio::IAudioSource* {
    for (auto& voice : voices_)
        if (voice && !voice->IsFinished())
            return voice.get();
    return voices_.empty() ? nullptr : voices_.front().get();
}
auto VoicePool::SetVoiceLimit(const std::size_t maximum) -> bool {
    if (!atom::numeric::IsValidVoiceCount(maximum))
        return false;
    maximum_ = maximum;
    // Existing voices are kept: the cap only gates how many new ones Acquire()
    // may create, so shrinking never destroys a voice that is still audible.
    return true;
}
} // namespace atom
