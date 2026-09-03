#ifndef ATOM_MUSIC_CROSSFADE_HPP
#define ATOM_MUSIC_CROSSFADE_HPP
#include <functional>
#include <string>
namespace atom {
class MusicPlayer;
}
namespace atom::audio {
enum class MusicTransitionState { Idle, FadingOut, FadingIn, Completed, Cancelled, Failed };
enum class FadeCurve { Linear, SmoothStep, EqualPower };
enum class TransitionConflictPolicy { Reject, Replace };
struct MusicCrossfadeConfig {
        float fade_out_duration = 1.0f;
        float fade_in_duration = 1.0f;
        FadeCurve curve = FadeCurve::EqualPower;
        TransitionConflictPolicy conflict_policy = TransitionConflictPolicy::Replace;
};
class MusicCrossfade final {
    public:
        using Callback = std::function<void(MusicTransitionState, const std::string&, const std::string&)>;
        explicit MusicCrossfade(MusicPlayer& player) : player_(player) {}
        auto Start(const std::string& target, const MusicCrossfadeConfig& config = {}) -> bool;
        auto Switch(const std::string& target, float duration) -> bool;
        auto Update(float delta_time) -> void;
        auto Cancel() -> void;
        auto Stop() -> void {
            Cancel();
        }
        auto Reset() -> void;
        auto SetCallback(Callback callback) -> void;
        [[nodiscard]] auto GetState() const -> MusicTransitionState;
        [[nodiscard]] auto GetProgress() const -> float;
        [[nodiscard]] auto GetFromId() const -> const std::string&;
        [[nodiscard]] auto GetToId() const -> const std::string&;
        [[nodiscard]] auto GetDuration() const -> float;
        [[nodiscard]] auto IsRunning() const -> bool;
        [[nodiscard]] auto IsFading() const -> bool {
            return IsRunning();
        }

    private:
        static auto EvaluateCurve(float progress, FadeCurve curve, bool fade_in) -> float;
        auto EnterState(MusicTransitionState state) -> void;
        auto Complete() -> void;
        MusicPlayer& player_;
        MusicTransitionState state_ = MusicTransitionState::Idle;
        MusicCrossfadeConfig config_{};
        std::string from_id_;
        std::string to_id_;
        float elapsed_ = 0.0f;
        float peak_volume_ = 100.0f;
        Callback callback_;
};
} // namespace atom::audio
#endif