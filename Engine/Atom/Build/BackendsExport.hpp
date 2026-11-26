#ifndef ATOM_BACKENDS_EXPORT_HPP
#define ATOM_BACKENDS_EXPORT_HPP

#if !defined(ATOM_BUILD_SHARED)
#define ATOM_BACKENDS_API
#elif defined(_WIN32) || defined(__CYGWIN__)
#if defined(ATOM_BACKENDS_EXPORTS)
#define ATOM_BACKENDS_API __declspec(dllexport)
#else
#define ATOM_BACKENDS_API __declspec(dllimport)
#endif
#else
#define ATOM_BACKENDS_API __attribute__((visibility("default")))
#endif

#endif // ATOM_BACKENDS_EXPORT_HPP
