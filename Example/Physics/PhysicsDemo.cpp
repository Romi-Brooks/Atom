// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file PhysicsDemo.cpp
 * @brief 物理模块交互式演示：用 Renderer2D 可视化一个刚体物理世界。
 *
 * 展示物理模块的完整用法：
 *   - 契约 + 自研 Internal 后端（Backend/Contracts/Physics + Backend/Physics/Internal）
 *   - FixedStepper 固定步进
 *   - 刚体（Static/Dynamic）+ 圆/凸多边形 + revolute 关节
 *   - 物理米制 + y 向上 → 渲染像素 + y 向下 的 PPM 换算
 *
 * 操作：
 *   鼠标左键  在光标处丢一个球
 *   R         重置场景
 *   Space     暂停/继续
 *   Esc       退出
 */

#include <array>
#include <cstddef>
#include <memory>
#include <variant>
#include <vector>

#include <Algorithm/Color/Color.hpp>
#include <Algorithm/Geometry/Rect.hpp>
#include <Algorithm/Vector/Vec2.hpp>
#include <Backend/Contracts/Physics/IPhysicsBody.hpp>
#include <Backend/Contracts/Physics/IPhysicsWorld.hpp>
#include <Backend/Contracts/Physics/PhysicsTypes.hpp>
#include <Backend/Contracts/Render/RenderBackendId.hpp>
#include <Backend/Physics/Internal/InternalPhysicsBackend.hpp>
#include <Concurrency/JobSystem.hpp>
#include <Debugger/FpsDebugger.hpp>
#include <Debugger/JobDebugger.hpp>
#include <Debugger/LogDebugger.hpp>
#include <Input/InputSystem.hpp>
#include <Input/Key.hpp>
#include <Log/LogSystem.hpp>
#include <Physics/Shapes/Shape.hpp>
#include <Render/Renderer2D/Renderer2D.hpp>
#include <Window/RenderWindow.hpp>
#include <Window/Screen.hpp>
#include <Window/ScreenManager.hpp>

namespace {

using atom::algo::Vec2;
using atom::color::Color;
using atom::physics::BodyDef;
using atom::physics::BodyType;
using atom::physics::IPhysicsBody;
using atom::physics::RevoluteJointDef;
using atom::physics::Shape;

// 每米像素数：把物理米制换算到渲染像素。
constexpr float kPpm = 50.0f;
// 渲染原点（屏幕坐标）：物理世界 (0,0) 映射到这里。
constexpr float kOriginX = 640.0f;
constexpr float kOriginY = 600.0f;
// 接触求解迭代次数（与原 FixedStepper{1/60, 8, 3} 一致）。
constexpr int kVelocityIterations = 8;
constexpr int kPositionIterations = 3;

[[nodiscard]] auto ToRenderX(const float x) -> float {
    return x * kPpm + kOriginX;
}
[[nodiscard]] auto ToRenderY(const float y) -> float {
    return kOriginY - y * kPpm;
}
[[nodiscard]] auto ToPhysicsX(const float x) -> float {
    return (x - kOriginX) / kPpm;
}
[[nodiscard]] auto ToPhysicsY(const float y) -> float {
    return (kOriginY - y) / kPpm;
}

[[nodiscard]] auto MakeSquare(const float half) -> Shape {
    const std::array<Vec2, 4> sq = {Vec2{-half, -half}, Vec2{half, -half}, Vec2{half, half}, Vec2{-half, half}};
    return *Shape::MakePolygon(sq);
}

// 一个待渲染的刚体：记录创建时的形状（局部空间）+ 颜色，渲染时用 body 的
// transform 变换到世界再换算到屏幕。
struct BodyEntry {
        IPhysicsBody* body = nullptr;
        Shape shape{};
        Color color{};
};

} // namespace

class PhysicsScreen final : public atom::Screen {
    public:
        PhysicsScreen() {
            ResetScene();
        }

        ~PhysicsScreen() override {
            if (renderer_.IsInitialized()) {
                renderer_.Shutdown();
            }
        }

        auto Render(atom::render::IRenderDevice& device) -> void override {
            if (!renderer_.IsInitialized() && !renderer_.Initialize(device, ATOM_SHADER_OUTPUT_DIR)) {
                return;
            }
            device.Clear(Color{12, 14, 22, 255});
            if (!renderer_.BeginFrame(0.0f, 0.0f, 1.0f)) {
                return;
            }
            DrawGround();
            for (const BodyEntry& entry : bodies_) {
                DrawBody(entry);
            }
            renderer_.EndFrame();
        }

        auto HandleEvent(const atom::window::IEvent& event) -> bool override {
            if (event.type == atom::window::EventType::KeyPressed) {
                const auto& key = std::get<atom::window::KeyEvent>(event.data);
                if (key.key == atom::input::Key::Escape) {
                    atom::RenderWindow::GetInstance().Shutdown();
                    return true;
                }
            }
            return false;
        }

        auto Update(const float /*delta_time*/) -> void override {
            auto& input = atom::input::InputSystem::GetInstance();

            if (input.IsKeyPressed(atom::input::Key::R)) {
                ResetScene();
            }
            if (input.IsKeyPressed(atom::input::Key::Space)) {
                paused_ = !paused_;
            }
            if (!paused_ && input.IsMousePressed(atom::input::MouseButton::Left)) {
                SpawnBall(input.GetMousePosition());
            }
            // 物理步进已移到 FixedUpdate（固定步域），Update 只处理输入与呈现。
        }

        auto FixedUpdate(const float step_seconds) -> void override {
            // 物理在 game 固定步域步进（默认 1/60s，每帧最多追 5 步）。引擎的
            // DomainClock 已做固定步累积 + 追步，这里直接用固定 dt 调 Step，
            // 不再需要 FixedStepper（原先二者重复）。
            if (!paused_ && world_) {
                world_->Step(step_seconds, kVelocityIterations, kPositionIterations);
            }
        }

    private:
        auto ResetScene() -> void {
            bodies_.clear();
            world_ = backend_.CreateWorld();
            if (!world_) {
                // Internal 后端不会失败，但契约允许返回空；调用方须处理这一 ERROR 路径。
                LOG_ERROR(atom::log::physics::Backend, "Failed to create physics world; scene reset aborted");
                return;
            }
            world_->SetGravity(Vec2{0.0f, -9.8f});

            // 地面（static，顶面在 y=0）。
            BodyDef ground{};
            ground.type = BodyType::Static;
            ground.shape = MakeSquare(12.0f);
            ground.position = Vec2{0.0f, -12.0f};
            ground.material.friction = 0.6f;
            ground_ = world_->CreateBody(ground);

            // 堆叠方块：从不同高度落下，落在彼此身上。
            (void)AddBox(Vec2{0.0f, 6.0f}, Color{70, 130, 220, 255});
            (void)AddBox(Vec2{0.9f, 8.0f}, Color{70, 130, 220, 255});
            (void)AddBox(Vec2{-0.9f, 10.0f}, Color{70, 130, 220, 255});

            // 圆球。
            (void)AddBall(Vec2{3.0f, 8.0f}, 0.5f, Color{240, 140, 60, 255});
            (void)AddBall(Vec2{4.0f, 10.0f}, 0.4f, Color{240, 180, 80, 255});

            // 单摆：静态锚点 + 球，球顶钉在锚点上。
            BodyDef anchor{};
            anchor.type = BodyType::Static;
            anchor.shape = *Shape::MakeCircle(0.15f);
            anchor.position = Vec2{-6.0f, 6.0f};
            IPhysicsBody* anchor_body = world_->CreateBody(anchor);
            bodies_.push_back({anchor_body, anchor.shape, Color{220, 220, 120, 255}});

            BodyDef bob{};
            bob.type = BodyType::Dynamic;
            bob.shape = *Shape::MakeCircle(0.4f);
            bob.material.friction = 0.4f;
            bob.position = Vec2{-6.0f, 5.6f};
            IPhysicsBody* bob_body = world_->CreateBody(bob);
            bob_body->SetLinearVelocity(Vec2{1.5f, 0.0f}); // 给一点水平初速，让它摆起来
            bodies_.push_back({bob_body, bob.shape, Color{80, 200, 120, 255}});

            RevoluteJointDef pendulum{};
            pendulum.local_anchor_a = Vec2{0.0f, 0.0f};
            pendulum.local_anchor_b = Vec2{0.0f, 0.4f}; // 球顶
            pendulum.collide_connected = false;
            world_->CreateRevoluteJoint(pendulum, anchor_body, bob_body);

            // 铰链链：两个方块铰接在一起。
            IPhysicsBody* link_a = AddBox(Vec2{6.0f, 6.0f}, Color{180, 120, 220, 255});
            IPhysicsBody* link_b = AddBox(Vec2{6.7f, 6.0f}, Color{180, 120, 220, 255});
            RevoluteJointDef chain{};
            chain.local_anchor_a = Vec2{0.4f, 0.0f};  // A 右边缘
            chain.local_anchor_b = Vec2{-0.4f, 0.0f}; // B 左边缘
            chain.collide_connected = false;
            world_->CreateRevoluteJoint(chain, link_a, link_b);
        }

        [[nodiscard]] auto AddBox(const Vec2 position, const Color color) -> IPhysicsBody* {
            BodyDef def{};
            def.type = BodyType::Dynamic;
            def.shape = MakeSquare(0.4f);
            def.material.friction = 0.4f;
            def.material.restitution = 0.1f;
            def.position = position;
            IPhysicsBody* body = world_->CreateBody(def);
            bodies_.push_back({body, def.shape, color});
            return body;
        }

        [[nodiscard]] auto AddBall(const Vec2 position, const float radius, const Color color) -> IPhysicsBody* {
            BodyDef def{};
            def.type = BodyType::Dynamic;
            def.shape = *Shape::MakeCircle(radius);
            def.material.friction = 0.4f;
            def.material.restitution = 0.3f;
            def.position = position;
            IPhysicsBody* body = world_->CreateBody(def);
            bodies_.push_back({body, def.shape, color});
            return body;
        }

        auto SpawnBall(const Vec2 mouse_render) -> void {
            const Vec2 position{ToPhysicsX(mouse_render.GetX()), ToPhysicsY(mouse_render.GetY())};
            const std::array<Color, 4> palette = {Color{240, 120, 120, 255}, Color{120, 220, 200, 255},
                                                  Color{230, 180, 90, 255}, Color{160, 160, 240, 255}};
            (void)AddBall(position, 0.35f, palette[ball_color_index_ % palette.size()]);
            ++ball_color_index_;
        }

        auto DrawGround() -> void {
            // 地面填充矩形：物理 x∈[-12,12]、顶面 y=0 → 屏幕一条横条。
            renderer_.DrawRect(atom::algo::Rect{ToRenderX(-12.0f), ToRenderY(0.0f), 24.0f * kPpm, 6.0f * kPpm},
                               Color{50, 55, 70, 255});
        }

        auto DrawBody(const BodyEntry& entry) -> void {
            const Vec2 position = entry.body->GetPosition();
            const float angle = entry.body->GetAngle();

            switch (entry.shape.GetKind()) {
            case Shape::Kind::Circle: {
                const auto& c = entry.shape.GetCircle();
                const Vec2 center = position + c.center.Rotated(angle);
                renderer_.DrawCircle(ToRenderX(center.GetX()), ToRenderY(center.GetY()), c.radius * kPpm,
                                     entry.color);
                break;
            }
            case Shape::Kind::Polygon: {
                // 旋转多边形：逐边画线（Renderer2D 尚无旋转矩形绘制，直接变换顶点）。
                const auto& p = entry.shape.GetPolygon();
                for (std::size_t i = 0; i < p.count; ++i) {
                    const Vec2 a = position + p.vertices[i].Rotated(angle);
                    const Vec2 b = position + p.vertices[(i + 1) % p.count].Rotated(angle);
                    renderer_.DrawLine(ToRenderX(a.GetX()), ToRenderY(a.GetY()), ToRenderX(b.GetX()),
                                       ToRenderY(b.GetY()), entry.color, 2.0f);
                }
                // 质心标记。
                renderer_.DrawCircle(ToRenderX(position.GetX()), ToRenderY(position.GetY()), 2.5f,
                                     Color{255, 255, 255, 255});
                break;
            }
            default:
                break; // Segment 不参与演示。
            }
        }

        atom::physics::InternalPhysicsBackend backend_{};
        std::unique_ptr<atom::physics::IPhysicsWorld> world_{};
        atom::render::Renderer2D renderer_{};
        std::vector<BodyEntry> bodies_{};
        IPhysicsBody* ground_ = nullptr;
        bool paused_ = false;
        int ball_color_index_ = 0;
};

auto main() -> int {
    atom::Log::SetConsoleOutputUtf8();
    atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_INFO);

    const auto loaded =
        atom::ScreenManager::GetInstance().LoadScreen("PhysicsDemo", std::make_unique<PhysicsScreen>());
    if (loaded != atom::ScreenOpResult::Success) {
        LOG_ERROR(atom::log::core::ScreenManager,
                  "Failed to register the physics demo screen: " + std::string{atom::ToString(loaded)});
        return 1;
    }
    atom::ScreenManager::GetInstance().SwitchScreen("PhysicsDemo");

    auto& window = atom::RenderWindow::GetInstance();
    const auto initialized = window.Initialize("Atom - Physics Demo", atom::algo::Vec2{1280.0f, 720.0f},
                                               atom::backend::RenderBackendId::SdlGpu);
    if (!initialized) {
        LOG_ERROR(atom::log::core::Main, "Window initialization failed: " + initialized.error().Message());
        return 1;
    }
    window.SetVSync(false);
    window.SetFPS(165);

    LOG_INFO(atom::log::core::Main,
             "Physics demo ready: click to drop a ball, R to reset, Space to pause, Esc to quit");

    // 调试面板：FPS（曲线）、日志、任务调度，三者都走共享 ImGui overlay 的
    // Attach，窗口级可见，无需示例自行维护。
    atom::debugger::FpsDebugger fps_panel{};
    fps_panel.Attach(window);

    atom::debugger::LogDebugger log_panel{};
    log_panel.Attach(window);

    atom::debugger::JobDebugger job_panel{};
    job_panel.Attach(window);

    // 物理模块当前是同步执行的：InternalPhysicsWorld::Step 直接在调用线程
    // （这里即渲染主线程）跑，尚未把宽/窄相位或求解拆到 JobSystem 上。这里
    // 启动调度器只是为了让 JobDebugger 显示一个存活的调度器；将来并行化
    // 后该面板即可实时反映物理作业的吞吐。
    auto& jobs = atom::job::JobSystem::GetInstance();
    if (!jobs.IsRunning()) {
        jobs.Configure();
        jobs.Start();
    }

    window.Run();
    jobs.Shutdown();
    return 0;
}
