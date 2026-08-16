/**
* @file           : GameChannels.hpp
  * @author         : Romi Brooks
  * @brief          : Game-side channel domains example (matches the game template
  *                   in Log/Doc/LogSystem.md, extended with nested domains)
  * @attention      : One line per channel; hierarchy via nested namespaces
  * @date           : 2026/9/20
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#pragma once

#include <Log/LogSystem.hpp>

// Level-1
ATOM_DEFINE_CHANNELS(game, GameLogChannel, "Game.",
    (GAME_NPC, "NPC"),
    (GAME_PLAYER, "Player"),
    (GAME_MAIN, "Main")
)

// Level-2
ATOM_DEFINE_CHANNELS(game::npc, LogChannel, "Game.NPC.",
    (DIALOG, "Dialog"),
    (AI, "AI")
)

// Level-3
ATOM_DEFINE_CHANNELS(game::npc::ai, LogChannel, "Game.NPC.AI.",
    (PATHFINDING, "Pathfinding"),
    (BEHAVIOR, "Behavior")
)
