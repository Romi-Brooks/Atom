#ifndef ATOM_HOMELESS_PROMENADESCREEN_HPP
#define ATOM_HOMELESS_PROMENADESCREEN_HPP

#include <filesystem>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <Atom/World/ECS/EntityWorld.hpp>
#include <Atom/Resources/Media/Audio/Mixing/AudioMixer.hpp>
#include <Atom/Presentation/Render/Resources/TextureCache.hpp>
#include <Atom/Presentation/Render/Scene2D/TextureCacheResolver.hpp>
#include <Atom/Runtime/Screen.hpp>
#include <Atom/Core/Utilities/Random/Rng.hpp>
#include <Atom/World/World2D/Animation2D.hpp>
#include <Atom/World/World2D/ParticleSystem2D.hpp>
#include <Atom/World/World2D/World2DRenderer.hpp>

#include "Pedestrians.hpp"
#include "SceneAudio.hpp"
#include "Traffic.hpp"
#include "UI/GameOverlay.hpp"

namespace homeless {

struct StreetEntrance;

class PromenadeScreen final : public atom::Screen {
    public:
        [[nodiscard]] static auto Create(atom::render::IRenderDevice& device, const std::filesystem::path& directory,
                                         unsigned frame_limit, bool& render_failed) -> std::unique_ptr<PromenadeScreen>;
        auto Render(atom::render::IRenderDevice& device) -> void override;
        auto HandleEvent(const atom::window::IEvent& event) -> bool override;
        auto BeginFrame(std::uint32_t, float, float alpha) -> void override {
            interpolation_ = alpha;
        }
        auto Update(float seconds) -> void override {
            scene_audio_.Update(mixer_.GetEffectiveSFXVolume());
            overlay_.Update(seconds);
        }
        auto FixedUpdate(float seconds) -> void override;
        auto OnActivate() -> void override;
        auto OnDeactivate() -> void override;

    private:
        enum class Area { Waterfront, Street };
        enum class Fade { None, Out, In };

        PromenadeScreen(std::unique_ptr<atom::ecs::EntityWorld> world, unsigned frame_limit, bool& render_failed,
                        atom::random::Rng scene_rng);
        auto SetUpWorld() -> bool;
        auto CheckAreaExit(int direction, float x) -> bool;
        auto UpdateAreaTransition(float seconds) -> void;
        auto SwitchArea() -> void;
        [[nodiscard]] auto NearbyEntrance() const -> const StreetEntrance*;
        auto UpdateWeather(float seconds) -> void;
        auto StepTraffic(float seconds) -> void;
        auto SetUpWeather() -> bool;
        auto SetUpWalkerAnimation() -> bool;
        auto UpdateWalker(float seconds) -> void;
        auto StepAnimation(float seconds) -> void;
        auto PlayFootstep() -> void;
        auto UpdateBreath(float seconds, bool walking) -> void;
        auto DrawWater(float left, float right) -> void;

        std::unique_ptr<atom::ecs::EntityWorld> world_;
        atom::render::Renderer2D renderer_{};
        atom::render::resources::TextureCache cache_{renderer_};
        atom::render::scene2d::TextureCacheResolver textures_;
        atom::world2d::World2DRenderer world_renderer_{textures_};
        atom::world2d::AnimationLibrary2D clips_{};
        atom::world2d::AnimationSystem2D animation_{clips_};
        atom::world2d::ParticleSystem2D snow_{2048, 2005};
        atom::AudioMixer mixer_{};
        SceneAudio scene_audio_{mixer_};
        GameOverlay overlay_{renderer_, mixer_};
        atom::random::Rng snow_rng_{2005};
        Traffic traffic_;
        Pedestrians pedestrians_;
        atom::random::Rng walker_rng_;

        atom::ecs::Entity walker_{};
        atom::ecs::Entity camera_{};
        std::size_t last_footstep_ = 0;
        atom::render::Renderer2D::Texture* waterfront_ = nullptr;
        atom::render::Renderer2D::Texture* street_ = nullptr;
        std::vector<atom::algo::Rect> water_spans_{};
        Area area_ = Area::Waterfront;
        Fade fade_ = Fade::None;
        float fade_opacity_ = 0.0f;
        float world_width_ = 0.0f;
        float interpolation_ = 0.0f;
        float walk_velocity_ = 0.0f;
        float weather_time_ = 0.0f;
        float next_breath_ = 0.8f;
        float far_snow_ = 0.0f;
        float near_snow_ = 0.0f;
        unsigned frames_remaining_ = 0;
        bool entrance_checked_ = false;
        bool& render_failed_;
};

} // namespace homeless

#endif // ATOM_HOMELESS_PROMENADESCREEN_HPP
