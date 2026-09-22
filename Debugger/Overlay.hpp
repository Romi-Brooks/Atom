/**
  * @file           : Overlay.hpp
  * @author         : Romi Brooks
  * @brief          : Engine-owned entry point for writing debug panels
  * @attention      : Re-exports ImGui + DebugPanel + PanelLayout. Game code
  *                   writes panels through this header instead of <imgui.h>
  *                   or Backend headers. Optional helpers:
  *                     #include <Debugger/LogDebugger.hpp>
  *                     #include <Debugger/FontLoader.hpp>
  *                   Panels own default geometry via PanelLayout — no imgui.ini.
  * @date           : 2026/8/16
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_DEBUGGER_OVERLAY_HPP
#define ATOM_DEBUGGER_OVERLAY_HPP

#include <imgui.h>
#include <Debugger/DebugPanel.hpp>
#include <Debugger/PanelLayout.hpp>

#endif // ATOM_DEBUGGER_OVERLAY_HPP
