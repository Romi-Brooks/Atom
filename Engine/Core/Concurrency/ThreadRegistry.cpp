// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include "ThreadRegistry.hpp"

#include <utility>

#ifdef _WIN32
// Kept out of the header: only this translation unit needs the Win32 thread
// API, mirroring LogSystem.cpp. WIN32_LEAN_AND_MEAN/NOMINMAX keep the macro
// surface small.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif // _WIN32

namespace atom::concurrency {
namespace {

// Cumulative CPU time (kernel + user) for a thread handle, in milliseconds.
// Returns 0 when the handle is invalid or the platform cannot report it.
auto ReadCpuMilliseconds(void* native_handle) -> double {
#ifdef _WIN32
    if (native_handle == nullptr)
        return 0.0;
    FILETIME creation{};
    FILETIME exit{};
    FILETIME kernel{};
    FILETIME user{};
    if (!GetThreadTimes(static_cast<HANDLE>(native_handle), &creation, &exit, &kernel, &user))
        return 0.0;
    const auto to_100ns = [](const FILETIME& ft) -> std::uint64_t {
        return (static_cast<std::uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    };
    // FILETIME is in 100 ns units; 10000 of them are one millisecond.
    return static_cast<double>(to_100ns(kernel) + to_100ns(user)) / 10000.0;
#else
    (void)native_handle;
    return 0.0;
#endif
}

} // namespace

ThreadRegistry::~ThreadRegistry() {
    std::lock_guard lock(mutex_);
#ifdef _WIN32
    for (auto& [_, entry] : entries_) {
        if (entry.native_handle)
            CloseHandle(static_cast<HANDLE>(entry.native_handle));
    }
#endif
    entries_.clear();
}

auto ThreadRegistry::GetInstance() -> ThreadRegistry& {
    static ThreadRegistry instance;
    return instance;
}

auto ThreadRegistry::Register(const std::string& name) -> void {
    Entry entry;
    entry.name = name;
    entry.id = std::this_thread::get_id();
    entry.alive = true;
#ifdef _WIN32
    // GetCurrentThread() returns a pseudo-handle valid only on this thread, so
    // duplicate it into a real process handle that another thread can query.
    HANDLE duplicated = nullptr;
    if (DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &duplicated, 0, FALSE,
                        DUPLICATE_SAME_ACCESS)) {
        entry.native_handle = duplicated;
    }
#endif
    std::lock_guard lock(mutex_);
    entries_[entry.id] = std::move(entry);
}

auto ThreadRegistry::Unregister() -> void {
    const auto id = std::this_thread::get_id();
    std::lock_guard lock(mutex_);
    const auto it = entries_.find(id);
    if (it == entries_.end())
        return;
#ifdef _WIN32
    if (it->second.native_handle)
        CloseHandle(static_cast<HANDLE>(it->second.native_handle));
#endif
    entries_.erase(it);
}

auto ThreadRegistry::Snapshot() -> std::vector<ThreadSnapshot> {
    std::vector<ThreadSnapshot> out;
    std::lock_guard lock(mutex_);
    out.reserve(entries_.size());
    for (const auto& [_, entry] : entries_) {
        ThreadSnapshot snapshot;
        snapshot.name = entry.name;
        snapshot.id = entry.id;
        snapshot.alive = entry.alive;
        snapshot.cpu_ms = ReadCpuMilliseconds(entry.native_handle);
        out.push_back(std::move(snapshot));
    }
    return out;
}

auto ThreadRegistry::LiveCount() -> std::size_t {
    std::lock_guard lock(mutex_);
    return entries_.size();
}

ThreadRegistry::ScopedRegistration::ScopedRegistration(std::string name) {
    ThreadRegistry::GetInstance().Register(std::move(name));
    active_ = true;
}

ThreadRegistry::ScopedRegistration::~ScopedRegistration() {
    if (active_) {
        ThreadRegistry::GetInstance().Unregister();
        active_ = false;
    }
}

} // namespace atom::concurrency
