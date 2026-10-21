/**
* @file           : AtomLogChannels.hpp
  * @author         : Romi Brooks
  * @brief          : Engine log channel form
  * @attention      : Normally included via LogSystem.hpp
  * @date           : 2025/9/20
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_ATOMLOGCHANNELS_HPP
#define ATOM_ATOMLOGCHANNELS_HPP

// Self Dependency
#include "LogChannelMacros.hpp"

// Engine log channel form. Design and usage: see Log/Doc/LogSystem.md.
// One ATOM_DEFINE_CHANNELS block per domain: (CppName, "DisplayName"),
// output = domain prefix + DisplayName + " -> ". Each domain holds up to 64 channels.

// Level-1
ATOM_DEFINE_CHANNELS(atom::log::core, Channel, "Atom.", (Main, "Main"), (Logger, "Logger"),
                     (Filesystem, "Filesystem"), (Lua, "Lua"), (Video, "Video"), (Window, "Window"),
                     (Screen, "Screen"), (ScreenManager, "Screen.Manager"), (Movement, "Movement"),
                     (Entity, "Entity"))

// Level-2
ATOM_DEFINE_CHANNELS(atom::log::entity, Channel, "Atom.Entity.", (Npc, "NPC"), (Player, "Player"))

// Level-2
// Asset loading failures (ResourceManager, resource loaders). One channel for
// the whole asset layer so a failing load is filterable independently of the
// subsystem that requested it.
ATOM_DEFINE_CHANNELS(atom::log::asset, Channel, "Atom.Asset.", (Resource, "Resource"))

// Level-2
ATOM_DEFINE_CHANNELS(atom::log::audio, Channel, "Atom.Audio.", (Music, "Music"), (Sfx, "SFX"),
                     (PlugMusicFade, "Plug.MusicFade"), (Minimp3, "Minimp3"), (WavProf, "WavProf"),
                     (SDL3Wav, "SDL3Wav"), (Metadata, "Metadata"))

// Level-2
ATOM_DEFINE_CHANNELS(atom::log::backend, Channel, "Atom.Backend.", (Runtime, "Runtime"))

// Level-2
ATOM_DEFINE_CHANNELS(atom::log::layout, Channel, "Atom.Layout.", (Core, "Core"))

// Level-2
ATOM_DEFINE_CHANNELS(atom::log::debugger, Channel, "Atom.Debugger.", (ImGui, "ImGui"))

// Level-2
ATOM_DEFINE_CHANNELS(atom::log::render, Channel, "Atom.Render.", (Renderer2D, "Renderer2D"))

// Level-2
ATOM_DEFINE_CHANNELS(atom::log::image, Channel, "Atom.Image.", (Decoder, "Decoder"))

// Level-3
// Backend channels are grouped by engine capability first, then by concrete
// backend identifier. This lets an audio-only log filter include every audio
// backend while keeping SDL3 and SDL3_mixer distinguishable.
ATOM_DEFINE_CHANNELS(atom::log::backend::Audio, Channel, "Atom.Backend.Audio.", (sdl3, "SDL3"),
                     (sdl3_mixer, "SDL3_mixer"))

// Legacy SDL3 backend categories that have not yet been moved to the
// capability-first hierarchy.
ATOM_DEFINE_CHANNELS(atom::log::backend::sdl3, Channel, "Atom.SDL3.Backend.", (Video, "Video"), (Render, "Render"),
                     (Window, "Window"))

// Level-2
ATOM_DEFINE_CHANNELS(atom::log::utilities, Channel, "Atom.Utilities.", (Packager, "Packager"))

// Level-2
ATOM_DEFINE_CHANNELS(atom::log::concurrency, Channel, "Atom.Concurrency.", (JobSystem, "JobSystem"))

// Level-2
// 物理模块日志。物理是高频计算，日志按级别收敛到「低频」边界，绝不在
// Step/接触/求解/逐刚体逐迭代的热路径打日志：
//   INFO    —— 生命周期与配置（world 创建/销毁、SetGravity、休眠开关）
//   WARNING —— 被拒绝的非法输入（零质量 dynamic、非法关节、销毁未归属的
//              刚体/关节、非有限重力、非法 Step 参数——非法 Step 参数只告警一次）
//   ERROR   —— 契约级灾难（后端创建 world 失败，由调用方处理）
//   DEBUG   —— 细粒度诊断（逐刚体创建/销毁、休眠/唤醒切换）
ATOM_DEFINE_CHANNELS(atom::log::physics, Channel, "Atom.Physics.",
                     (World, "World"), (Body, "Body"), (Backend, "Backend"))

#endif // ATOM_ATOMLOGCHANNELS_HPP
