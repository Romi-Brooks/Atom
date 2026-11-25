#include <memory>

#include <Atom/Core/Concurrency/JobSystem.hpp>
#include <Atom/Core/Log/LogSystem.hpp>
#include <Atom/Runtime/Screen.hpp>
#include <Atom/Runtime/ScreenManager.hpp>

namespace {
class ExitScreen final : public atom::Screen {
    public:
        ~ExitScreen() override {
            LOG_INFO(atom::log::core::Main, "Atom SDK released the registered screen");
        }

        auto Render(atom::render::IRenderDevice&) -> void override {}
        auto HandleEvent(const atom::window::IEvent&) -> bool override {
            return false;
        }
        auto Update(float) -> void override {}
};
} // namespace

auto main() -> int {
    atom::Log::SetConsoleSinkEnabled(true);
    auto& screens = atom::ScreenManager::GetInstance();
    if (screens.LoadScreen("sdk-smoke", std::make_unique<ExitScreen>()) != atom::ScreenOpResult::Success) {
        return 1;
    }
    auto& jobs = atom::job::JobSystem::GetInstance();
    jobs.Configure(1);
    if (!jobs.Start()) {
        return 1;
    }

    // Process exit must join the scheduler and flush the worker's last log.
    const auto job = jobs.Schedule([] { LOG_INFO(atom::log::core::Main, "Atom SDK smoke reached exit"); });
    return job.IsValid() ? 0 : 2;
}
