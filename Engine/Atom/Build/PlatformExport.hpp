#ifndef ATOM_PLATFORM_EXPORT_HPP
#define ATOM_PLATFORM_EXPORT_HPP

#if !defined(ATOM_BUILD_SHARED)
#define ATOM_PLATFORM_API
#elif defined(_WIN32) || defined(__CYGWIN__)
#if defined(ATOM_PLATFORM_EXPORTS)
#define ATOM_PLATFORM_API __declspec(dllexport)
#else
#define ATOM_PLATFORM_API __declspec(dllimport)
#endif
#else
#define ATOM_PLATFORM_API __attribute__((visibility("default")))
#endif

#endif // ATOM_PLATFORM_EXPORT_HPP
