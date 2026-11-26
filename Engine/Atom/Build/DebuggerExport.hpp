#ifndef ATOM_DEBUGGER_EXPORT_HPP
#define ATOM_DEBUGGER_EXPORT_HPP

#if !defined(ATOM_BUILD_SHARED)
#define ATOM_DEBUGGER_API
#elif defined(_WIN32) || defined(__CYGWIN__)
#if defined(ATOM_DEBUGGER_EXPORTS)
#define ATOM_DEBUGGER_API __declspec(dllexport)
#else
#define ATOM_DEBUGGER_API __declspec(dllimport)
#endif
#else
#define ATOM_DEBUGGER_API __attribute__((visibility("default")))
#endif

#endif // ATOM_DEBUGGER_EXPORT_HPP
