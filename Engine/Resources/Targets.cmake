# Codecs parse media formats into PCM; they are not playback backends, so this
# target stays platform-neutral (no SDL3 link). The SDL-backed WAV reader lives
# with the SDL3 audio backend in Engine/Backend/SDL3/Audio.
add_library(Atom_Audio_Codecs STATIC
    Engine/Resources/Media/Audio/Codecs/WavProf/RiffWaveReader.hpp
    Engine/Resources/Media/Audio/Codecs/WavProf/RiffWaveReader.cpp
    Engine/Resources/Media/Audio/Codecs/WavProf/WavProfDecoder.hpp
    Engine/Resources/Media/Audio/Codecs/WavProf/WavProfDecoder.cpp
    Engine/Resources/Media/Audio/Codecs/minimp3/Minimp3Decoder.hpp
    Engine/Resources/Media/Audio/Codecs/minimp3/Minimp3Decoder.cpp
)
target_include_directories(Atom_Audio_Codecs PUBLIC "${PROJECT_SOURCE_DIR}/Engine/Resources")
target_link_libraries(Atom_Audio_Codecs PUBLIC Atom_Backend_Contracts Atom_Backend_Extension Atom_3rd_Minimp3 Atom_UTF8 Atom_FS)

# Compatibility target for out-of-tree consumers. New CMake code must use
# Atom_Audio_Codecs so the target name matches its Engine/Resources/Media/Audio location.
add_library(Atom_Backend_Audio INTERFACE)
target_link_libraries(Atom_Backend_Audio INTERFACE Atom_Audio_Codecs)

# Media
add_library(Atom_Media STATIC
    Engine/Resources/Media/Audio/AudioBackend.cpp
    Engine/Resources/Media/Audio/AudioBackend.hpp
    Engine/Resources/Media/Audio/Metadata/AudioMetadataReader.cpp
    Engine/Resources/Media/Audio/Metadata/AudioMetadataReader.hpp
    Engine/Resources/Media/Audio/Effects/IAudioEffect.hpp
    Engine/Resources/Media/Audio/Effects/DopplerEffect.cpp
    Engine/Resources/Media/Audio/Effects/DopplerEffect.hpp
    Engine/Resources/Media/Audio/Mixing/AudioMixer.cpp
    Engine/Resources/Media/Audio/Mixing/AudioMixer.hpp
    Engine/Resources/Media/Audio/Resources/AudioClipLoader.cpp
    Engine/Resources/Media/Audio/Resources/AudioClipLoader.hpp
    Engine/Resources/Media/Audio/Resources/AudioClipResourceLoader.cpp
    Engine/Resources/Media/Audio/Resources/AudioClipResourceLoader.hpp
    Engine/Resources/Media/Audio/Resources/AudioClipCache.cpp
    Engine/Resources/Media/Audio/Resources/AudioClipCache.hpp
    Engine/Resources/Media/Audio/Playback/MusicPlayer.cpp
    Engine/Resources/Media/Audio/Playback/MusicPlayer.hpp
    Engine/Resources/Media/Audio/Playback/SFXPlayer.cpp
    Engine/Resources/Media/Audio/Playback/SFXPlayer.hpp
    Engine/Resources/Media/Audio/Playback/VoicePool.cpp
    Engine/Resources/Media/Audio/Playback/VoicePool.hpp
    Engine/Resources/Media/Audio/Transitions/MusicCrossfade.cpp
    Engine/Resources/Media/Audio/Transitions/MusicCrossfade.hpp
)
target_include_directories(Atom_Media PUBLIC "${PROJECT_SOURCE_DIR}/Engine/Resources")
target_link_libraries(Atom_Media PUBLIC Atom_Backend_Contracts Atom_Backend_Extension Atom_Backend_Composition Atom_Concurrency Atom_Log Atom_UTF8 Atom_3rd_TagLib Atom_FS Atom_Assets)

# Platform-independent virtual filesystem. Native implementation details
# remain private; consumers use AssetPath and IFileSystem only.
add_library(Atom_FS STATIC
    Engine/Resources/Filesystem/AssetPath.hpp
    Engine/Resources/Filesystem/AssetPath.cpp
    Engine/Resources/Filesystem/FileSystem.hpp
    Engine/Resources/Filesystem/FileSystem.cpp
    Engine/Resources/Filesystem/MemoryFileSystem.hpp
    Engine/Resources/Filesystem/MemoryFileSystem.cpp
    Engine/Resources/Filesystem/OverlayFileSystem.hpp
    Engine/Resources/Filesystem/OverlayFileSystem.cpp
    Engine/Resources/Filesystem/Vfs.hpp
    Engine/Resources/Filesystem/Vfs.cpp
    Engine/Resources/Filesystem/PackageFormat.hpp
    Engine/Resources/Filesystem/PackageFileSystem.hpp
    Engine/Resources/Filesystem/PackageFileSystem.cpp
)
target_include_directories(Atom_FS PUBLIC "${PROJECT_SOURCE_DIR}/Engine/Resources")
# Atom_Error is PUBLIC: AssetPath::Parse returns Result<AssetPath>, so every
# consumer of the FS public headers needs the error type to be complete.
target_link_libraries(Atom_FS PUBLIC Atom_Error)
target_link_libraries(Atom_FS PRIVATE Atom_UTF8)

# Resource identity, handles, loader registry, and identity-keyed cache.
add_library(Atom_Assets STATIC
    Engine/Resources/Asset/AssetKind.hpp
    Engine/Resources/Asset/AssetKind.cpp
    Engine/Resources/Asset/ResourceId.hpp
    Engine/Resources/Asset/ResourceId.cpp
    Engine/Resources/Asset/ResourceHandle.hpp
    Engine/Resources/Asset/IResourceLoader.hpp
    Engine/Resources/Asset/IResourceLoader.cpp
    Engine/Resources/Asset/ResourceManager.hpp
    Engine/Resources/Asset/ResourceManager.cpp
    Engine/Resources/Asset/ScriptSourceLoader.hpp
    Engine/Resources/Asset/ScriptSourceLoader.cpp
)
target_include_directories(Atom_Assets PUBLIC "${PROJECT_SOURCE_DIR}/Engine/Resources")
# Atom_Log is needed because a failed acquire reports itself exactly once, at
# the site that knows the operation and the resource (CORE-004).
target_link_libraries(Atom_Assets PUBLIC Atom_FS Atom_Log)
# Batch acquire spreads the per-file load (I/O + decode) across the job system,
# so ResourceManager depends on the scheduler (implementation detail, not API).
target_link_libraries(Atom_Assets PRIVATE Atom_Concurrency)

# Image decoding (CPU-side; GPU upload is a separate step via Renderer2D).
add_library(Atom_Image STATIC
    Engine/Resources/Media/Image/ImageDecoder.hpp
    Engine/Resources/Media/Image/ImageDecoder.cpp
        Engine/Resources/Media/Image/Analysis/ColorStatistics.hpp
        Engine/Resources/Media/Image/Analysis/ColorStatistics.cpp
    Engine/Resources/Media/Image/DecodedImageLoader.hpp
    Engine/Resources/Media/Image/DecodedImageLoader.cpp
    Engine/Resources/Media/Image/ImageBatchDecoder.hpp
    Engine/Resources/Media/Image/ImageBatchDecoder.cpp
)
target_include_directories(Atom_Image PUBLIC "${PROJECT_SOURCE_DIR}/Engine/Resources")
target_link_libraries(Atom_Image PUBLIC Atom_Color Atom_Assets)
target_link_libraries(Atom_Image PRIVATE Atom_3rd_Stb Atom_Log Atom_Concurrency Atom_Backend_Contracts)

# Packager
add_library(Atom_Packager STATIC
        Engine/Resources/Utilities/Packager/PackageBuilder.cpp
        Engine/Resources/Utilities/Packager/PackageBuilder.hpp
)
target_include_directories(Atom_Packager PUBLIC "${PROJECT_SOURCE_DIR}/Engine/Resources" "${PROJECT_SOURCE_DIR}/Engine/Resources/Utilities/Packager")
target_link_libraries(Atom_Packager PUBLIC Atom_UTF8 Atom_Log PRIVATE Atom_FS)
