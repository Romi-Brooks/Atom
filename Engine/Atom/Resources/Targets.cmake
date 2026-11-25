# Codecs parse media formats into PCM; they are not playback backends, so this
# target stays platform-neutral (no SDL3 link). The SDL-backed WAV reader lives
# with the SDL3 audio backend in Engine/Atom/Backend/SDL3/Audio.
add_library(Atom_Audio_Codecs STATIC
    Engine/Atom/Resources/Media/Audio/Codecs/WavProf/RiffWaveReader.hpp
    Engine/Atom/Resources/Media/Audio/Codecs/WavProf/RiffWaveReader.cpp
    Engine/Atom/Resources/Media/Audio/Codecs/WavProf/WavProfDecoder.hpp
    Engine/Atom/Resources/Media/Audio/Codecs/WavProf/WavProfDecoder.cpp
    Engine/Atom/Resources/Media/Audio/Codecs/minimp3/Minimp3Decoder.hpp
    Engine/Atom/Resources/Media/Audio/Codecs/minimp3/Minimp3Decoder.cpp
)
target_include_directories(Atom_Audio_Codecs PUBLIC "${PROJECT_SOURCE_DIR}/Engine")
target_link_libraries(Atom_Audio_Codecs PUBLIC Atom_Backend_Contracts Atom_Backend_Extension Atom_3rd_Minimp3 Atom_UTF8 Atom_FS)

# Compatibility target for out-of-tree consumers. New CMake code must use
# Atom_Audio_Codecs so the target name matches its Engine/Atom/Resources/Media/Audio location.
add_library(Atom_Backend_Audio INTERFACE)
target_link_libraries(Atom_Backend_Audio INTERFACE Atom_Audio_Codecs)

# Media
add_library(Atom_Media STATIC
    Engine/Atom/Resources/Media/Audio/AudioBackend.cpp
    Engine/Atom/Resources/Media/Audio/AudioBackend.hpp
    Engine/Atom/Resources/Media/Audio/Metadata/AudioMetadataReader.cpp
    Engine/Atom/Resources/Media/Audio/Metadata/AudioMetadataReader.hpp
    Engine/Atom/Resources/Media/Audio/Effects/IAudioEffect.hpp
    Engine/Atom/Resources/Media/Audio/Mixing/AudioMixer.cpp
    Engine/Atom/Resources/Media/Audio/Mixing/AudioMixer.hpp
    Engine/Atom/Resources/Media/Audio/Resources/AudioClipLoader.cpp
    Engine/Atom/Resources/Media/Audio/Resources/AudioClipLoader.hpp
    Engine/Atom/Resources/Media/Audio/Resources/AudioClipResourceLoader.cpp
    Engine/Atom/Resources/Media/Audio/Resources/AudioClipResourceLoader.hpp
    Engine/Atom/Resources/Media/Audio/Resources/AudioClipCache.cpp
    Engine/Atom/Resources/Media/Audio/Resources/AudioClipCache.hpp
    Engine/Atom/Resources/Media/Audio/Playback/MusicPlayer.cpp
    Engine/Atom/Resources/Media/Audio/Playback/MusicPlayer.hpp
    Engine/Atom/Resources/Media/Audio/Playback/SFXPlayer.cpp
    Engine/Atom/Resources/Media/Audio/Playback/SFXPlayer.hpp
    Engine/Atom/Resources/Media/Audio/Playback/VoicePool.cpp
    Engine/Atom/Resources/Media/Audio/Playback/VoicePool.hpp
    Engine/Atom/Resources/Media/Audio/Transitions/MusicCrossfade.cpp
    Engine/Atom/Resources/Media/Audio/Transitions/MusicCrossfade.hpp
)
target_include_directories(Atom_Media PUBLIC "${PROJECT_SOURCE_DIR}/Engine")
target_link_libraries(Atom_Media PUBLIC Atom_Backend_Contracts Atom_Backend_Extension Atom_Backend_Composition Atom_Concurrency Atom_Log Atom_UTF8 Atom_3rd_TagLib Atom_FS Atom_Assets)

# Platform-independent virtual filesystem. Native implementation details
# remain private; consumers use AssetPath and IFileSystem only.
add_library(Atom_FS STATIC
    Engine/Atom/Resources/Filesystem/AssetPath.hpp
    Engine/Atom/Resources/Filesystem/AssetPath.cpp
    Engine/Atom/Resources/Filesystem/FileSystem.hpp
    Engine/Atom/Resources/Filesystem/FileSystem.cpp
    Engine/Atom/Resources/Filesystem/MemoryFileSystem.hpp
    Engine/Atom/Resources/Filesystem/MemoryFileSystem.cpp
    Engine/Atom/Resources/Filesystem/OverlayFileSystem.hpp
    Engine/Atom/Resources/Filesystem/OverlayFileSystem.cpp
    Engine/Atom/Resources/Filesystem/Vfs.hpp
    Engine/Atom/Resources/Filesystem/Vfs.cpp
    Engine/Atom/Resources/Filesystem/PackageFormat.hpp
    Engine/Atom/Resources/Filesystem/PackageFileSystem.hpp
    Engine/Atom/Resources/Filesystem/PackageFileSystem.cpp
)
target_include_directories(Atom_FS PUBLIC "${PROJECT_SOURCE_DIR}/Engine")
# Atom_Error is PUBLIC: AssetPath::Parse returns Result<AssetPath>, so every
# consumer of the FS public headers needs the error type to be complete.
target_link_libraries(Atom_FS PUBLIC Atom_Error)
target_link_libraries(Atom_FS PRIVATE Atom_UTF8)

# Resource identity, handles, loader registry, and identity-keyed cache.
add_library(Atom_Assets STATIC
    Engine/Atom/Resources/Asset/AssetKind.hpp
    Engine/Atom/Resources/Asset/AssetKind.cpp
    Engine/Atom/Resources/Asset/ResourceId.hpp
    Engine/Atom/Resources/Asset/ResourceId.cpp
    Engine/Atom/Resources/Asset/ResourceHandle.hpp
    Engine/Atom/Resources/Asset/IResourceLoader.hpp
    Engine/Atom/Resources/Asset/IResourceLoader.cpp
    Engine/Atom/Resources/Asset/ResourceManager.hpp
    Engine/Atom/Resources/Asset/ResourceManager.cpp
    Engine/Atom/Resources/Asset/ScriptSourceLoader.hpp
    Engine/Atom/Resources/Asset/ScriptSourceLoader.cpp
)
target_include_directories(Atom_Assets PUBLIC "${PROJECT_SOURCE_DIR}/Engine")
# Atom_Log is needed because a failed acquire reports itself exactly once, at
# the site that knows the operation and the resource (CORE-004).
target_link_libraries(Atom_Assets PUBLIC Atom_FS Atom_Log)
# Batch acquire spreads the per-file load (I/O + decode) across the job system,
# so ResourceManager depends on the scheduler (implementation detail, not API).
target_link_libraries(Atom_Assets PRIVATE Atom_Concurrency)

# Image decoding (CPU-side; GPU upload is a separate step via Renderer2D).
add_library(Atom_Image STATIC
    Engine/Atom/Resources/Media/Image/ImageDecoder.hpp
    Engine/Atom/Resources/Media/Image/ImageDecoder.cpp
        Engine/Atom/Resources/Media/Image/Analysis/ColorStatistics.hpp
        Engine/Atom/Resources/Media/Image/Analysis/ColorStatistics.cpp
    Engine/Atom/Resources/Media/Image/DecodedImageLoader.hpp
    Engine/Atom/Resources/Media/Image/DecodedImageLoader.cpp
    Engine/Atom/Resources/Media/Image/ImageBatchDecoder.hpp
    Engine/Atom/Resources/Media/Image/ImageBatchDecoder.cpp
)
target_include_directories(Atom_Image PUBLIC "${PROJECT_SOURCE_DIR}/Engine")
target_link_libraries(Atom_Image PUBLIC Atom_Color Atom_Assets)
target_link_libraries(Atom_Image PRIVATE Atom_3rd_Stb Atom_Log Atom_Concurrency Atom_Backend_Contracts)

# Packager
add_library(Atom_Packager STATIC
        Engine/Atom/Resources/Utilities/Packager/PackageBuilder.cpp
        Engine/Atom/Resources/Utilities/Packager/PackageBuilder.hpp
)
target_include_directories(Atom_Packager PUBLIC "${PROJECT_SOURCE_DIR}/Engine")
target_link_libraries(Atom_Packager PUBLIC Atom_UTF8 Atom_Log PRIVATE Atom_FS)
