#ifndef ATOM_MEDIA_AUDIO_EXPORT_HPP
#define ATOM_MEDIA_AUDIO_EXPORT_HPP

#if !defined(ATOM_BUILD_SHARED)
#define ATOM_MEDIA_AUDIO_API
#elif defined(_WIN32) || defined(__CYGWIN__)
#if defined(ATOM_MEDIA_AUDIO_EXPORTS)
#define ATOM_MEDIA_AUDIO_API __declspec(dllexport)
#else
#define ATOM_MEDIA_AUDIO_API __declspec(dllimport)
#endif
#else
#define ATOM_MEDIA_AUDIO_API __attribute__((visibility("default")))
#endif

#endif // ATOM_MEDIA_AUDIO_EXPORT_HPP
