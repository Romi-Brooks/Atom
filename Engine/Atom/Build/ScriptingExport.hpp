#ifndef ATOM_SCRIPTING_EXPORT_HPP
#define ATOM_SCRIPTING_EXPORT_HPP

#if !defined(ATOM_BUILD_SHARED)
#define ATOM_SCRIPTING_API
#elif defined(_WIN32) || defined(__CYGWIN__)
#if defined(ATOM_SCRIPTING_EXPORTS)
#define ATOM_SCRIPTING_API __declspec(dllexport)
#else
#define ATOM_SCRIPTING_API __declspec(dllimport)
#endif
#else
#define ATOM_SCRIPTING_API __attribute__((visibility("default")))
#endif

#endif // ATOM_SCRIPTING_EXPORT_HPP
