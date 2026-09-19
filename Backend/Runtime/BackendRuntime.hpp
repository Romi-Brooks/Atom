#ifndef ATOM_BACKEND_RUNTIME_BACKEND_RUNTIME_HPP
#define ATOM_BACKEND_RUNTIME_BACKEND_RUNTIME_HPP

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <Backend/Extension/AudioDecoderRegistry.hpp>
#include <Backend/Extension/BackendRegistry.hpp>

namespace atom::audio {
class IAudioBackend;
}

namespace atom::backend {

class IAudioBackendChangeListener;

class BackendRuntime final {
    public:
        static auto GetInstance() -> BackendRuntime&;
        ~BackendRuntime();

        BackendRuntime(const BackendRuntime&) = delete;
        auto operator=(const BackendRuntime&) -> BackendRuntime& = delete;

        // Active playback backend. Never null and never throws: while no backend
        // is active these return the NullAudioBackend, whose factories report
        // failure. Use TryAudio() when "no backend" is a case worth handling.
        [[nodiscard]] auto Audio() -> audio::IAudioBackend&;
        [[nodiscard]] auto TryAudio() -> audio::IAudioBackend*;

        // Strong reference to the active backend. Source creation must go through
        // this: it keeps the backend object (and therefore the platform subsystem
        // it leases) alive for the whole call, so a loader thread that raced a
        // backend switch cannot create a source on an already-destroyed backend.
        // Returns nullptr only if no backend is active.
        [[nodiscard]] auto AcquireAudioBackend() -> std::shared_ptr<audio::IAudioBackend>;

        [[nodiscard]] auto AudioDecoders() -> audio::AudioDecoderRegistry&;
        [[nodiscard]] auto Registry() -> BackendRegistry&;

        // Global switch for the playback backend. The replacement backend is
        // created first, so a failed switch leaves the active backend (and every
        // source it owns) untouched. On success the change is applied in three
        // steps: listeners release their sources, the old backend detaches
        // whatever is left, then the swap happens. No playback position is
        // migrated and no ID is re-registered -- callers must Load/Play again
        // against the new backend. See README-CN.md.
        auto SetAudioBackend(std::string_view id) -> bool;
        [[nodiscard]] auto GetAudioBackendId() const -> const std::string&;

        // Monotonic counter, incremented on every successful switch. Any id,
        // source or cache produced by an older generation is stale: the backend
        // that created it is gone. Owners that cache playback ids should stamp
        // them with this value and re-resolve on mismatch.
        [[nodiscard]] auto GetAudioBackendGeneration() const -> std::uint64_t;

        // Register the engine's built-in format decoders (.wav, .mp3, ...) into an
        // AudioDecoderRegistry. Used by the global runtime; also callable for
        // explicitly injected registries (tests / standalone tools).
        static auto RegisterDefaultAudioDecoders(audio::AudioDecoderRegistry& decoders) -> void;

        auto AddAudioListener(IAudioBackendChangeListener& listener) -> void;
        auto RemoveAudioListener(IAudioBackendChangeListener& listener) -> void;

    private:
        BackendRuntime();

        auto RegisterAvailableBackends() -> void;
        auto NotifyAudioBackendChanging() -> void;
        auto NotifyAudioBackendChanged() -> void;

        BackendRegistry registry_;
        audio::AudioDecoderRegistry audio_decoders_;
        mutable std::mutex backend_mutex_;
        std::shared_ptr<audio::IAudioBackend> audio_backend_;
        // Returned by Audio() when no backend is active. Only reachable before the
        // constructor finished; kept so the accessor can never throw.
        std::shared_ptr<audio::IAudioBackend> null_backend_;
        std::string audio_backend_id_;
        std::uint64_t audio_backend_generation_ = 0;
        bool reported_missing_backend_ = false;
        std::vector<IAudioBackendChangeListener*> audio_listeners_;
};

} // namespace atom::backend

#endif
