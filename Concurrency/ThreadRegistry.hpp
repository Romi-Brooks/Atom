// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file ThreadRegistry.hpp
 * @brief Process-wide registry of long-lived engine threads for diagnostics.
 *
 * A debugger (or a telemetry log line) uses this to answer "which threads are
 * alive right now, and which one is burning the most CPU?". Each participating
 * thread drops a ScopedRegistration at the top of its entry function; the
 * registry records its name, id and — on Windows — a duplicated native handle
 * so another thread can read its cumulative CPU time via GetThreadTimes.
 *
 * This is diagnostics-only and must never sit on a hot path. Threads that do
 * not register simply do not appear. The registry is deliberately a leaf
 * module (no dependency on Log or the job system) so even Log's own console
 * writer thread can register without creating a dependency cycle.
 */

#ifndef ATOM_CONCURRENCY_THREAD_REGISTRY_HPP
#define ATOM_CONCURRENCY_THREAD_REGISTRY_HPP

#include <cstddef>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace atom::concurrency {

/// One registered thread, safe to copy out to a debugger panel or telemetry.
struct ThreadSnapshot {
        std::string name;
        std::thread::id id;
        bool alive = false;
        /// Cumulative CPU time (user + kernel) in milliseconds since the thread
        /// registered. 0 on platforms without per-thread CPU accounting.
        double cpu_ms = 0.0;
};

/// Registry of the engine's long-lived threads. A process-wide singleton.
class ThreadRegistry final {
    public:
        ThreadRegistry(const ThreadRegistry&) = delete;
        auto operator=(const ThreadRegistry&) -> ThreadRegistry& = delete;

        [[nodiscard]] static auto GetInstance() -> ThreadRegistry&;

        /// RAII guard: construct at the top of a thread's entry function.
        /// The thread is unregistered when the guard is destroyed (including
        /// via stack unwinding on an exception).
        class ScopedRegistration final {
            public:
                explicit ScopedRegistration(std::string name);
                ~ScopedRegistration();
                ScopedRegistration(const ScopedRegistration&) = delete;
                auto operator=(const ScopedRegistration&) -> ScopedRegistration& = delete;
                ScopedRegistration(ScopedRegistration&&) = delete;
                auto operator=(ScopedRegistration&&) -> ScopedRegistration& = delete;

            private:
                bool active_ = false;
        };

        /// Snapshot of every thread currently registered, for a debugger panel.
        [[nodiscard]] auto Snapshot() -> std::vector<ThreadSnapshot>;

        /// Number of threads currently registered.
        [[nodiscard]] auto LiveCount() -> std::size_t;

    private:
        struct Entry {
                std::string name;
                std::thread::id id;
                void* native_handle = nullptr; // duplicated thread handle (Windows)
                bool alive = false;
        };

        ThreadRegistry() = default;
        ~ThreadRegistry();

        auto Register(const std::string& name) -> void;
        auto Unregister() -> void;

        friend class ScopedRegistration;

        mutable std::mutex mutex_;
        std::unordered_map<std::thread::id, Entry> entries_;
};

} // namespace atom::concurrency

#endif // ATOM_CONCURRENCY_THREAD_REGISTRY_HPP
