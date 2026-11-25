#ifndef ATOM_WORLD_EXPORT_HPP
#define ATOM_WORLD_EXPORT_HPP

#if !defined(ATOM_BUILD_SHARED)
#define ATOM_WORLD_API
#elif defined(_WIN32) || defined(__CYGWIN__)
#if defined(ATOM_WORLD_EXPORTS)
#define ATOM_WORLD_API __declspec(dllexport)
#else
#define ATOM_WORLD_API __declspec(dllimport)
#endif
#else
#define ATOM_WORLD_API __attribute__((visibility("default")))
#endif

#endif // ATOM_WORLD_EXPORT_HPP
