/**
  * @file           : LogChannelDemo.cpp
  * @author         : Romi Brooks
  * @brief          : Log channel domain demo (one log per section, levels spread
  *                   across domains; per-domain enumeration + level filtering):
  *                   - Level-1 domain: atom::log::core (prefix "Atom.")
  *                   - Level-2 domains: atom::log::audio / atom::log::entity
  *                   - Level-3 domain: atom::log::backend::Audio
  *                   - Game domain: game::log (prefix "Game."), nested
  *                     game::log::npc (Level-2) and game::log::npc::ai (Level-3)
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

namespace {
// Print every channel of a domain: "prefix + shortName"
template <typename TChannel> auto PrintAllChannels(const char* title) -> void {
    std::cout << "  " << title << ":" << std::endl;
    for (std::size_t i = 0; i < static_cast<std::size_t>(TChannel::COUNT); ++i) {
        const auto channel = static_cast<TChannel>(i);
        std::cout << "    " << GetChannelPrefix(channel) << GetChannelName(channel) << std::endl;
    }
}
} // namespace

auto main() -> int {
    atom::Log::SetConsoleOutputUtf8();
    // Set the log level first
    atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_DEBUG);

    std::cout << "===== Level-1 domain: atom::log::core =====" << std::endl;
    LOG_INFO(atom::log::core::Main, "Engine booting...");

    std::cout << "===== Level-2 domain: atom::log::audio =====" << std::endl;
    LOG_WARNING(atom::log::audio::Sfx, "SFX not found");

    std::cout << "===== Level-2 domain: atom::log::entity =====" << std::endl;
    LOG_ERROR(atom::log::entity::Player, "Player save failed");

    std::cout << "===== Level-3 domain: atom::log::backend::Audio =====" << std::endl;
    LOG_DEBUG(atom::log::backend::Audio::sdl3, "Audio backend initialized");

    std::cout << "===== Game domain: game::log =====" << std::endl;
    LOG_INFO(game::log::Npc, "NPC spawned");

    std::cout << "===== Game Level-2 domain: game::log::npc =====" << std::endl;
    LOG_WARNING(game::log::npc::Ai, "AI state reset");

    std::cout << "===== Game Level-3 domain: game::log::npc::ai =====" << std::endl;
    LOG_ERROR(game::log::npc::ai::Pathfinding, "Pathfinding failed");

    std::cout << "===== Ad-hoc string channel =====" << std::endl;
    LOG_INFO("Game.NPC", "ad-hoc: dialog started");

    PrintAllChannels<atom::log::core::Channel>("all atom::log::core channels");
    PrintAllChannels<atom::log::audio::Channel>("all atom::log::audio channels");
    PrintAllChannels<atom::log::entity::Channel>("all atom::log::entity channels");
    PrintAllChannels<atom::log::backend::Audio::Channel>("all atom::log::backend::Audio channels");
    PrintAllChannels<game::log::Channel>("all game::log channels");
    PrintAllChannels<game::log::npc::Channel>("all game::log::npc channels");
    PrintAllChannels<game::log::npc::ai::Channel>("all game::log::npc::ai channels");

    return 0;
}
