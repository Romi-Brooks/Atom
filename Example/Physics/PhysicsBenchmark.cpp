// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file PhysicsBenchmark.cpp
 * @brief Headless, repeatable baseline for deciding whether physics phases merit JobSystem work.
 *
 * Run from a Release build, for example:
 *   Example_Physics_Benchmark --bodies 1024 --steps 600 --spacing 0.85 --jobs
 *
 * spacing below the 0.9 m box width produces a contact-heavy scene. A value
 * above 0.9 keeps bodies separated while still exercising broad phase.
 */

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string_view>

#include <Algorithm/Vector/Vec2.hpp>
#include <Physics/Contracts/IPhysicsWorld.hpp>
#include <Physics/Contracts/PhysicsTypes.hpp>
#include <Physics/Backend/Internal/InternalPhysicsBackend.hpp>
#include <Concurrency/JobSystem.hpp>
#include <Physics/Shapes/Shape.hpp>

namespace {

using atom::algo::Vec2;
using atom::physics::BodyDef;
using atom::physics::BodyType;
using atom::physics::InternalPhysicsBackend;
using atom::physics::Shape;

struct Config final {
        std::size_t bodies = 1024;
        std::size_t warmup_steps = 60;
        std::size_t measured_steps = 600;
        float spacing = 0.85f;
        bool use_jobs = false;
};

[[nodiscard]] auto ParseSize(const std::string_view text, std::size_t& out) -> bool {
    char* end = nullptr;
    const auto value = std::strtoull(text.data(), &end, 10);
    if (end != text.data() + text.size() || value == 0) {
        return false;
    }
    out = static_cast<std::size_t>(value);
    return true;
}

[[nodiscard]] auto ParseFloat(const std::string_view text, float& out) -> bool {
    char* end = nullptr;
    const auto value = std::strtof(text.data(), &end);
    if (end != text.data() + text.size() || !std::isfinite(value) || value <= 0.0f) {
        return false;
    }
    out = value;
    return true;
}

[[nodiscard]] auto ParseArgs(const int argc, char** argv, Config& config) -> bool {
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg{argv[i]};
        if (arg == "--bodies" && i + 1 < argc) {
            if (!ParseSize(argv[++i], config.bodies)) {
                return false;
            }
        } else if (arg == "--warmup" && i + 1 < argc) {
            if (!ParseSize(argv[++i], config.warmup_steps)) {
                return false;
            }
        } else if (arg == "--steps" && i + 1 < argc) {
            if (!ParseSize(argv[++i], config.measured_steps)) {
                return false;
            }
        } else if (arg == "--spacing" && i + 1 < argc) {
            if (!ParseFloat(argv[++i], config.spacing)) {
                return false;
            }
        } else if (arg == "--jobs") {
            config.use_jobs = true;
        } else {
            return false;
        }
    }
    return true;
}

auto PrintUsage() -> void {
    std::cerr
        << "Usage: Example_Physics_Benchmark [--bodies N] [--warmup N] [--steps N] [--spacing meters] [--jobs]\n";
}

} // namespace

auto main(const int argc, char** argv) -> int {
    Config config{};
    if (!ParseArgs(argc, argv, config)) {
        PrintUsage();
        return 2;
    }

    InternalPhysicsBackend backend{};
    auto world = backend.CreateWorld();
    if (!world) {
        std::cerr << "Failed to create physics world\n";
        return 1;
    }
    world->SetGravity(Vec2::Zero());
    world->SetSleepingEnabled(false);

    const std::array<Vec2, 4> square = {
        Vec2{-0.45f, -0.45f}, Vec2{0.45f, -0.45f}, Vec2{0.45f, 0.45f}, Vec2{-0.45f, 0.45f}};
    const auto shape = Shape::MakePolygon(square);
    if (!shape) {
        std::cerr << "Failed to create benchmark shape\n";
        return 1;
    }

    const auto columns = static_cast<std::size_t>(std::ceil(std::sqrt(static_cast<double>(config.bodies))));
    for (std::size_t index = 0; index < config.bodies; ++index) {
        BodyDef body{};
        body.type = BodyType::Dynamic;
        body.shape = *shape;
        body.material.friction = 0.4f;
        body.position = Vec2{static_cast<float>(index % columns) * config.spacing,
                             static_cast<float>(index / columns) * config.spacing};
        if (world->CreateBody(body) == nullptr) {
            std::cerr << "Failed to create benchmark body " << index << '\n';
            return 1;
        }
    }

    auto& jobs = atom::job::JobSystem::GetInstance();
    if (config.use_jobs) {
        jobs.Configure();
        if (!jobs.Start()) {
            std::cerr << "Failed to start JobSystem\n";
            return 1;
        }
    }

    constexpr float kStep = 1.0f / 60.0f;
    for (std::size_t step = 0; step < config.warmup_steps; ++step) {
        world->Step(kStep, 8, 3);
    }

    const auto start = std::chrono::steady_clock::now();
    for (std::size_t step = 0; step < config.measured_steps; ++step) {
        world->Step(kStep, 8, 3);
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const double elapsed_ms = std::chrono::duration<double, std::milli>(elapsed).count();

    std::cout << std::fixed << std::setprecision(3)
              << "Physics benchmark: bodies=" << config.bodies << ", warmup=" << config.warmup_steps
              << ", measured=" << config.measured_steps << ", spacing=" << config.spacing
              << " m, jobs=" << (config.use_jobs ? "on" : "off") << "\n"
              << "Step total=" << elapsed_ms << " ms, average=" << elapsed_ms / config.measured_steps
              << " ms/step\n";
    if (config.use_jobs) {
        jobs.Shutdown();
    }
    return 0;
}
