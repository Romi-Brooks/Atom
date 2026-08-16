/**
  * @file           : LogChannelDemo.cpp
  * @author         : Romi Brooks
  * @brief          : Log channel domain demo (one log per section, levels spread
  *                   across domains; per-domain enumeration + level filtering):
  *                   - Level-1 domain: atom::core::LogChannel (prefix "Atom.")
  *                   - Level-2 domains: atom::audio / atom::entity
  *                   - Level-3 domain: atom::backend::sdl
  *                   - Game domain: game::GameLogChannel (prefix "Game."),
  *                     nested game::npc (Level-2) and game::npc::ai (Level-3)
  *                   - Ad-hoc string channels, per-domain enumeration,
  *                     level filtering
  * @attention      :
  * @date           : 2026/9/20
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include <cstddef>
#include <iostream>

#include <Log/LogSystem.hpp>

#include "GameChannels.hpp"

#ifdef _WIN32
#include "windows.h"
#endif // _WIN32

namespace {
// Print every channel of a domain: "prefix + shortName"
template <typename TChannel>
auto PrintAllChannels(const char* title) -> void {
    std::cout << "  " << title << ":" << std::endl;
    for (std::size_t i = 0; i < static_cast<std::size_t>(TChannel::COUNT); ++i) {
        const auto channel = static_cast<TChannel>(i);
        std::cout << "    " << GetChannelPrefix(channel) << GetChannelName(channel) << std::endl;
    }
}
} // namespace

auto main() -> int {
    #ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    #endif // _WIN32

    // Set the log level first
    atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_DEBUG);

    std::cout << "===== Level-1 domain: atom::core::LogChannel =====" << std::endl;
    LOG_INFO(atom::core::LogChannel::MAIN, "Engine booting...");

    std::cout << "===== Level-2 domain: atom::audio::LogChannel =====" << std::endl;
    LOG_WARNING(atom::audio::LogChannel::SFX, "SFX not found");

    std::cout << "===== Level-2 domain: atom::entity::LogChannel =====" << std::endl;
    LOG_ERROR(atom::entity::LogChannel::PLAYER, "Player save failed");

    std::cout << "===== Level-3 domain: atom::backend::sdl::LogChannel =====" << std::endl;
    LOG_DEBUG(atom::backend::sdl::LogChannel::RENDER, "SDL renderer created");

    std::cout << "===== Game domain: game::GameLogChannel =====" << std::endl;
    LOG_INFO(game::GameLogChannel::GAME_NPC, "NPC spawned");

    std::cout << "===== Game Level-2 domain: game::npc::LogChannel =====" << std::endl;
    LOG_WARNING(game::npc::LogChannel::AI, "AI state reset");

    std::cout << "===== Game Level-3 domain: game::npc::ai::LogChannel =====" << std::endl;
    LOG_ERROR(game::npc::ai::LogChannel::PATHFINDING, "Pathfinding failed");

    std::cout << "===== Ad-hoc string channel =====" << std::endl;
    LOG_INFO("Game.NPC", "ad-hoc: dialog started");

    PrintAllChannels<atom::core::LogChannel>("all atom::core channels");
    PrintAllChannels<atom::audio::LogChannel>("all atom::audio channels");
    PrintAllChannels<atom::entity::LogChannel>("all atom::entity channels");
    PrintAllChannels<atom::backend::sdl::LogChannel>("all atom::backend::sdl channels");
    PrintAllChannels<game::GameLogChannel>("all game channels");
    PrintAllChannels<game::npc::LogChannel>("all game::npc channels");
    PrintAllChannels<game::npc::ai::LogChannel>("all game::npc::ai channels");

    return 0;
}
