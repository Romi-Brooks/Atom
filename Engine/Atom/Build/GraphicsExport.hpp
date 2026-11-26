#ifndef ATOM_GRAPHICS_EXPORT_HPP
#define ATOM_GRAPHICS_EXPORT_HPP

#if !defined(ATOM_BUILD_SHARED)
#define ATOM_GRAPHICS_API
#elif defined(_WIN32) || defined(__CYGWIN__)
#if defined(ATOM_GRAPHICS_EXPORTS)
#define ATOM_GRAPHICS_API __declspec(dllexport)
#else
#define ATOM_GRAPHICS_API __declspec(dllimport)
#endif
#else
#define ATOM_GRAPHICS_API __attribute__((visibility("default")))
#endif

#endif // ATOM_GRAPHICS_EXPORT_HPP
