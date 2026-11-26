#ifndef ATOM_RUNTIME_EXPORT_HPP
#define ATOM_RUNTIME_EXPORT_HPP

#if !defined(ATOM_BUILD_SHARED)
#define ATOM_RUNTIME_API
#elif defined(_WIN32) || defined(__CYGWIN__)
#if defined(ATOM_RUNTIME_EXPORTS)
#define ATOM_RUNTIME_API __declspec(dllexport)
#else
#define ATOM_RUNTIME_API __declspec(dllimport)
#endif
#else
#define ATOM_RUNTIME_API __attribute__((visibility("default")))
#endif

#endif // ATOM_RUNTIME_EXPORT_HPP
