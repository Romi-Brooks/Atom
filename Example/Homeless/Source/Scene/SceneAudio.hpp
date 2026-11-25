#ifndef ATOM_HOMELESS_SCENEAUDIO_HPP
#define ATOM_HOMELESS_SCENEAUDIO_HPP

#include <array>
#include <cstddef>

#include <Concurrency/Job.hpp>
#include <Media/Audio/Playback/MusicPlayer.hpp>
#include <Media/Audio/Playback/SFXPlayer.hpp>

namespace homeless {

class SceneAudio final {
    public:
        enum class Sound { Footstep1, Footstep2, Footstep3, PassingCar };
        static constexpr std::size_t footstep_count = 3;

        explicit SceneAudio(atom::AudioMixer& mixer) : ambience_(mixer), effects_(clips_, mixer) {}
        ~SceneAudio();

        auto Start() -> void {
            active_ = true;
        }
        auto Stop() -> void;
        auto Update(float volume) -> void;
        auto Queue(Sound sound) -> bool;

    private:
        auto Load(float volume) -> void;

        atom::MusicPlayer ambience_;
        atom::AudioClipCache clips_{};
        atom::SFXPlayer effects_;
        atom::job::JobHandle job_{};
        std::array<Sound, 8> pending_{};
        std::size_t count_ = 0;
        float last_volume_ = -1.0f;
        bool active_ = false;
        bool started_ = false;
};

} // namespace homeless

#endif // ATOM_HOMELESS_SCENEAUDIO_HPP
