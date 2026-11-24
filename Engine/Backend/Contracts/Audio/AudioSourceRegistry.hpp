#ifndef ATOM_AUDIO_SOURCE_REGISTRY_HPP
#define ATOM_AUDIO_SOURCE_REGISTRY_HPP

#include <cstddef>
#include <mutex>
#include <unordered_set>
#include <vector>

#include <Backend/Contracts/Audio/IAudioSource.hpp>

namespace atom::audio {

// Registry of every live source created by one playback backend.
//
// Backends own process-wide resources: destroying an SDL3 audio backend runs
// SDL_QuitSubSystem(SDL_INIT_AUDIO), which destroys every SDL_AudioStream in the
// process, and the SDL_mixer context takes the mixer down with it. A source that
// survives that teardown keeps dangling handles and crashes on its next call (or
// in its destructor). BackendRuntime therefore calls IAudioBackend::Quiesce()
// right before a backend is destroyed, and the backend forwards it to
// DetachAll() so every source releases its handles first.
//
// Track()/Untrack() are driven by BackendOwnedSource. Backend switching is
// documented as a main-thread operation; the mutex only protects the
// bookkeeping, not the sources themselves.
class AudioSourceRegistry final {
    public:
        auto Track(IAudioSource* source) -> void {
            if (source == nullptr) {
                return;
            }
            std::scoped_lock lock{mutex_};
            sources_.insert(source);
        }

        auto Untrack(IAudioSource* source) -> void {
            std::scoped_lock lock{mutex_};
            sources_.erase(source);
        }

        // Detaches every source that is still tracked. Idempotent: the set is
        // cleared before Detach() runs, so a source that unbinds itself (or is
        // destroyed) re-entrantly cannot corrupt the iteration.
        auto DetachAll() -> void {
            std::vector<IAudioSource*> sources;
            {
                std::scoped_lock lock{mutex_};
                sources.assign(sources_.begin(), sources_.end());
                sources_.clear();
            }
            for (auto* source : sources) {
                if (source != nullptr) {
                    source->Detach();
                }
            }
        }

        [[nodiscard]] auto TrackedCount() const -> std::size_t {
            std::scoped_lock lock{mutex_};
            return sources_.size();
        }

    private:
        mutable std::mutex mutex_;
        std::unordered_set<IAudioSource*> sources_;
};

// Base class for sources that are owned by a playback backend.
//
// It keeps the backend's AudioSourceRegistry in sync and turns IAudioSource
// ::Detach() into a single hook the derived source must implement. Deriving
// sources only have to call BindRegistry() once (normally in the backend
// factory right after construction) and implement ReleaseBackendHandles().
class BackendOwnedSource : public IAudioSource {
    public:
        BackendOwnedSource() = default;
        ~BackendOwnedSource() override {
            UnbindFromRegistry();
        }

        BackendOwnedSource(const BackendOwnedSource&) = delete;
        auto operator=(const BackendOwnedSource&) -> BackendOwnedSource& = delete;
        BackendOwnedSource(BackendOwnedSource&&) = delete;
        auto operator=(BackendOwnedSource&&) -> BackendOwnedSource& = delete;

        // Final on purpose: every backend-owned source detaches through the same
        // path. Backend-specific teardown lives in ReleaseBackendHandles().
        auto Detach() -> void final {
            UnbindFromRegistry();
            ReleaseBackendHandles();
        }

        // Called once by the creating backend, normally right after
        // construction: makes the source visible to Quiesce()/DetachAll().
        auto BindRegistry(AudioSourceRegistry& registry) -> void {
            registry_ = &registry;
            registry.Track(this);
        }

    protected:
        // Release every backend handle (and join every worker thread) so the
        // source becomes inert and safe to destroy. Must be idempotent, and must
        // also be safe to call when the source was only partially constructed.
        virtual auto ReleaseBackendHandles() -> void = 0;

    private:
        auto UnbindFromRegistry() -> void {
            if (registry_ == nullptr) {
                return;
            }
            auto* registry = registry_;
            registry_ = nullptr;
            registry->Untrack(this);
        }

        AudioSourceRegistry* registry_ = nullptr;
};

} // namespace atom::audio

#endif // ATOM_AUDIO_SOURCE_REGISTRY_HPP
