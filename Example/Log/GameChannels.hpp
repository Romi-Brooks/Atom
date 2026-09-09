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
ATOM_DEFINE_CHANNELS(game::log, Channel, "Game.",
    (Npc, "NPC"),
    (Player, "Player"),
    (Main, "Main")
)

// Level-2
ATOM_DEFINE_CHANNELS(game::log::npc, Channel, "Game.NPC.",
    (Dialog, "Dialog"),
    (Ai, "AI")
)

// Level-3
ATOM_DEFINE_CHANNELS(game::log::npc::ai, Channel, "Game.NPC.AI.",
    (Pathfinding, "Pathfinding"),
    (Behavior, "Behavior")
)
