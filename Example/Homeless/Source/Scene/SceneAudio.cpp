#include "SceneAudio.hpp"

#include <string>
#include <utility>
#include <vector>

#include <Concurrency/JobSystem.hpp>
#include <Log/LogSystem.hpp>

namespace homeless {

namespace {

constexpr std::array ids{"snow-step-1", "snow-step-2", "snow-step-3", "passing-car"};
constexpr std::array paths{"res://Audio/Footstep1.wav", "res://Audio/Footstep2.wav", "res://Audio/Footstep3.wav",
                           "res://Audio/PassingCar.mp3"};
constexpr std::array gains{0.23f, 0.23f, 0.23f, 0.18f};
const std::string ambience_id = "waterfront";
constexpr float ambience_gain = 0.34f;

} // namespace

SceneAudio::~SceneAudio() {
    job_.Wait();
}

auto SceneAudio::Load(float volume) -> void {
    (void)effects_.SetVoiceLimit(4);
    for (std::size_t i = 0; i < ids.size(); ++i)
        if (!effects_.Load(ids[i], paths[i]))
            LOG_WARNING(atom::log::audio::Sfx, "Homeless sound is unavailable: " + std::string(paths[i]));
    if (ambience_.Load(ambience_id, "res://Audio/Waterfront.wav")) {
        ambience_.SetLooping(ambience_id, true);
        ambience_.Play(ambience_id, volume * ambience_gain);
    } else {
        LOG_WARNING(atom::log::audio::Sfx, "Waterfront ambience is unavailable");
    }
}

auto SceneAudio::Queue(Sound sound) -> bool {
    if (!active_ || count_ >= pending_.size())
        return false;
    pending_[count_++] = sound;
    return true;
}

auto SceneAudio::Update(float volume) -> void {
    if (!active_ || (job_.IsValid() && !job_.IsComplete()))
        return;
    const bool volume_changed = volume != last_volume_;
    if (volume <= 0)
        count_ = 0;
    if (started_ && !volume_changed && count_ == 0)
        return;

    job_ = atom::job::JobSystem::GetInstance().Schedule(
        "Homeless: scene audio",
        [this, initialize = !started_, volume_changed, volume, sounds = pending_, count = count_] {
            if (initialize)
                Load(volume);
            else if (volume_changed) {
                ambience_.SetVolume(ambience_id, volume * ambience_gain);
                for (std::size_t i = 0; i < ids.size(); ++i)
                    effects_.SetVolume(ids[i], volume * gains[i]);
            }
            for (std::size_t i = 0; i < count; ++i) {
                const auto sound = static_cast<std::size_t>(sounds[i]);
                effects_.Play(ids[sound], volume * gains[sound]);
            }
        });
    started_ = job_.IsValid();
    last_volume_ = volume;
    count_ = 0;
    if (!started_)
        active_ = false;
}

auto SceneAudio::Stop() -> void {
    active_ = false;
    count_ = 0;
    if (!started_)
        return;
    started_ = false;
    std::vector<atom::job::JobHandle> dependencies{job_};
    auto stop = atom::job::JobSystem::GetInstance().Schedule(
        "Homeless: stop scene audio",
        [this] {
            ambience_.Stop(ambience_id);
            effects_.StopAll();
        },
        atom::job::Priority::Normal, std::move(dependencies));
    if (stop.IsValid())
        job_ = std::move(stop);
}

} // namespace homeless
