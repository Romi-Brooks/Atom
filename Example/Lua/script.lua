-- LuaScripting example script.
-- Demonstrates driving the engine through the Atom.Audio.* bindings.
-- The MusicPlayer is attached to the Lua context, so Load/Play here actually
-- schedule a real track on the active audio backend.

-- The music directory is mounted as res:// by the host (see LuaScripting.cpp).
-- Replace the filename with whatever is in your music folder.
local track = "res://我的歌声里 - 曲婉婷.mp3"

-- Atom.Audio.Music.Load returns a boolean: false means the track could not be
-- found or decoded. We degrade gracefully instead of erroring out, so the
-- example still runs even if that exact file is not in the mounted directory.
local loaded = Atom.Audio.Music.Load("song_1", track)
if loaded then
    Atom.Audio.Music.Play("song_1", 80.0)
    Atom.Log.Info("Lua loaded and started playback of: " .. track)
else
    -- Fallback: the track is missing; report it and keep going. The host side
    -- (LuaScripting.cpp) also logs a warning when 'song_1' is not registered.
    Atom.Log.Warning("Lua: track not found under res://, skipping playback (" .. track .. ")")
end

-- Mixer is attached too: adjust the music bus.
Atom.Audio.Mixer.SetMasterVolume(90.0)
Atom.Audio.Mixer.SetMusicVolume(70.0)

local master = Atom.Audio.Mixer.GetMasterVolume()
local music  = Atom.Audio.Mixer.GetMusicVolume()
Atom.Log.Info(string.format("Lua set mixer: master=%.1f music=%.1f", master, music))
