#ifndef ATOM_CORE_EXPORT_HPP
#define ATOM_CORE_EXPORT_HPP

#if !defined(ATOM_BUILD_SHARED)
#define ATOM_CORE_API
#elif defined(_WIN32) || defined(__CYGWIN__)
#if defined(ATOM_CORE_EXPORTS)
#define ATOM_CORE_API __declspec(dllexport)
#else
#define ATOM_CORE_API __declspec(dllimport)
#endif
#else
#define ATOM_CORE_API __attribute__((visibility("default")))
#endif

#endif // ATOM_CORE_EXPORT_HPP
