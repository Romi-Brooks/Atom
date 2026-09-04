/**
  * @file           : Overlay.hpp
  * @author         : Romi Brooks
  * @brief          : Engine-owned entry point for debug overlays
  * @attention      : Re-exports the bundled ImGui API together with the
  *                   overlay base class. Game code should include this header
  *                   instead of <imgui.h> / Backend headers; the engine owns
  *                   the ImGui dependency and its backend wiring.
  * @date           : 2026/8/16
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_WINDOW_OVERLAY_HPP
#define ATOM_WINDOW_OVERLAY_HPP

#include <imgui.h>
#include <Window/Debugger.hpp>
#include <Window/LogDebugger.hpp>
#include <Window/Debugger/ImGui/ImGuiFontLoader.hpp>

#endif // ATOM_WINDOW_OVERLAY_HPP
