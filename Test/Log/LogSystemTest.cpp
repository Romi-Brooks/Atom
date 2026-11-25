#include <Atom/Core/Log/LogSystem.hpp>
#include <Test/Support/TestHelpers.hpp>

#include <atomic>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

auto main() -> int {
    const auto channels = atom::Log::GetRegisteredChannels();
    ATOM_CHECK(!channels.empty());
    bool saw_core = false;
    for (const auto& channel : channels) {
        if (channel.name == "Screen.Manager" || channel.name.find("Screen") != std::string::npos)
            saw_core = true;
    }
    ATOM_CHECK(saw_core);

    ATOM_CHECK(std::string_view{atom::ToString(atom::LogLevel::ATOM_DEBUG)} == "DEBUG");
    ATOM_CHECK(std::string_view{atom::ToString(atom::LogLevel::ATOM_INFO)} == "INFO");
    ATOM_CHECK(std::string_view{atom::ToString(atom::LogLevel::ATOM_WARNING)} == "WARNING");
    ATOM_CHECK(std::string_view{atom::ToString(atom::LogLevel::ATOM_ERROR)} == "ERROR");

    // Keep test output quiet; listeners still receive every level. Console
    // output is decoupled from delivery, so silencing it cannot change the
    // assertions below.
    atom::Log::SetConsoleSinkEnabled(false);
    ATOM_CHECK(!atom::Log::IsConsoleSinkEnabled());

    // ARCH-110: the console threshold is atomic and observable from any thread.
    atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_ERROR);
    ATOM_CHECK(atom::Log::GetViewLogLevel() == atom::LogLevel::ATOM_ERROR);

    // --- ARCH-110: records emitted before Subscribe() are replayed ---------
    atom::Log::SetRecentCapacity(8);
    atom::Log::ClearRecentRecords();
    LOG_INFO(atom::log::core::Main, "startup-a");
    LOG_WARNING(atom::log::core::Main, "startup-b");
    ATOM_CHECK(atom::Log::GetRecentRecords().size() == 2);

    std::vector<atom::LogRecord> records{};
    auto connection = atom::Log::Subscribe([&records](const atom::LogRecord& record) { records.push_back(record); });
    ATOM_CHECK(connection.IsConnected());
    // The two pre-subscription records arrive as a replay, in emission order.
    ATOM_CHECK(records.size() == 2);
    ATOM_CHECK(records[0].message == "startup-a");
    ATOM_CHECK(records[1].message == "startup-b");
    ATOM_CHECK(records[1].level == atom::LogLevel::ATOM_WARNING);
    ATOM_CHECK(!records[0].timestamp.empty());
    ATOM_CHECK(!records[0].channel_name.empty());
    ATOM_CHECK(!records[0].file.empty());
    ATOM_CHECK(records[0].line > 0);
    ATOM_CHECK(!records[0].function.empty());

    // Live records keep flowing after the replay.
    LOG_ERROR(atom::log::core::Main, "live-error");
    ATOM_CHECK(records.size() == 3);
    ATOM_CHECK(records.back().message == "live-error");
    ATOM_CHECK(records.back().level == atom::LogLevel::ATOM_ERROR);

    // The ring is bounded: the oldest record is dropped once it is full.
    atom::Log::SetRecentCapacity(2);
    LOG_INFO(atom::log::core::Main, "ring-1");
    LOG_INFO(atom::log::core::Main, "ring-2");
    const auto buffered = atom::Log::GetRecentRecords();
    ATOM_CHECK(buffered.size() == 2);
    ATOM_CHECK(buffered[0].message == "ring-1");
    ATOM_CHECK(buffered[1].message == "ring-2");

    connection.Reset();
    ATOM_CHECK(!connection.IsConnected());
    const auto count_after_reset = records.size();
    LOG_INFO(atom::log::core::Main, "after-disconnect");
    ATOM_CHECK(records.size() == count_after_reset);

    auto empty = atom::Log::Subscribe({});
    empty.Reset();
    ATOM_CHECK(!empty.IsConnected());

    // --- ARCH-110: concurrent logging stays consistent ---------------------
    atom::Log::SetRecentCapacity(4096);
    atom::Log::ClearRecentRecords();
    constexpr int thread_count = 4;
    constexpr int records_per_thread = 250;
    {
        std::atomic<int> delivered{0};
        auto concurrent = atom::Log::Subscribe([&delivered](const atom::LogRecord&) { delivered.fetch_add(1); });
        std::vector<std::thread> threads{};
        threads.reserve(thread_count);
        for (int t = 0; t < thread_count; ++t) {
            threads.emplace_back([t] {
                for (int i = 0; i < records_per_thread; ++i)
                    LOG_WARNING(atom::log::core::Main,
                                "concurrent-" + std::to_string(t) + '-' + std::to_string(i));
            });
        }
        for (auto& thread : threads)
            thread.join();
        // No record is lost and none is duplicated across threads.
        ATOM_CHECK(delivered.load() == thread_count * records_per_thread);
        concurrent.Reset();
    }

    // Thread-safe time formatting: every concurrent record has a well-formed
    // timestamp (the old std::localtime() race produced empty/garbled ones).
    const std::string expected_shape = "0000-00-00 00:00:00.000";
    const auto concurrent_records = atom::Log::GetRecentRecords();
    ATOM_CHECK(concurrent_records.size() == static_cast<std::size_t>(thread_count * records_per_thread));
    for (const auto& record : concurrent_records) {
        ATOM_CHECK(record.timestamp.size() == expected_shape.size());
        ATOM_CHECK(record.timestamp[4] == '-');
        ATOM_CHECK(record.timestamp[10] == ' ');
        ATOM_CHECK(record.timestamp[13] == ':');
    }

    // --- console sink -------------------------------------------------------
    // The view level is ERROR for the whole test, so these use LOG_ERROR: the
    // console sink is gated by the view level, while listeners are not.
    //
    // FlushConsole() must not return while a line is still in the writer's hand,
    // so a flushed queue means the pending count really is zero.
    atom::Log::SetConsoleQueueCapacity(atom::Log::default_console_queue_capacity);
    atom::Log::SetConsoleSinkEnabled(true);
    LOG_ERROR(atom::log::core::Main, "sink-smoke");
    atom::Log::FlushConsole();
    ATOM_CHECK(atom::Log::GetPendingConsoleLineCount() == 0);

    // The bounded producer: with a one-line queue, a burst from several threads
    // must never leave more than one line pending, and must never block. Whether
    // a particular line was dropped is timing-dependent, so the deterministic
    // assertion is the bound, not the counter.
    atom::Log::SetConsoleQueueCapacity(1);
    {
        std::vector<std::thread> producers{};
        producers.reserve(4);
        for (int t = 0; t < 4; ++t) {
            producers.emplace_back([] {
                const std::string padding(200, 'x');
                for (int i = 0; i < 200; ++i)
                    LOG_ERROR(atom::log::core::Main, "console-overflow-" + std::to_string(i) + padding);
            });
        }
        for (auto& producer : producers)
            producer.join();
    }
    ATOM_CHECK(atom::Log::GetPendingConsoleLineCount() <= 1);
    atom::Log::FlushConsole();
    ATOM_CHECK(atom::Log::GetPendingConsoleLineCount() == 0);

    // The degenerate bound is fully deterministic: a zero-length queue accepts
    // nothing and counts every suppressed line.
    atom::Log::SetConsoleQueueCapacity(0);
    const auto dropped_before = atom::Log::GetDroppedConsoleLineCount();
    LOG_ERROR(atom::log::core::Main, "dropped-1");
    LOG_ERROR(atom::log::core::Main, "dropped-2");
    ATOM_CHECK(atom::Log::GetPendingConsoleLineCount() == 0);
    ATOM_CHECK(atom::Log::GetDroppedConsoleLineCount() == dropped_before + 2);

    atom::Log::SetConsoleQueueCapacity(atom::Log::default_console_queue_capacity);
    atom::Log::SetConsoleSinkEnabled(false);
    atom::Log::FlushConsole();

    // --- file sink ----------------------------------------------------------
    // One switch, one file per run: Log/<YYYYMMDD-HHMMSS>.log relative to the
    // working directory, with no prefix in front of the stamp. The test reads
    // the file back, so it also pins "the file carries the same lines as the
    // console" and "turning the sink off does not swallow the queued tail".
    const auto read_text = [](const std::string& path) {
        std::ifstream stream{path, std::ios::binary};
        return std::string{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
    };

    ATOM_CHECK(!atom::Log::IsFileOutputEnabled());
    ATOM_CHECK(atom::Log::GetFileOutputPath().empty());

    atom::Log::SetFileOutput(true);
    ATOM_CHECK(atom::Log::IsFileOutputEnabled());
    const std::string first_path = atom::Log::GetFileOutputPath();
    ATOM_CHECK(!first_path.empty());

    const std::filesystem::path first_file{first_path};
    ATOM_CHECK(first_file.parent_path().filename() == "Log");
    ATOM_CHECK(first_file.extension() == ".log");
    const std::string first_stem = first_file.stem().string();
    // The name starts with the 15-character stamp YYYYMMDD-HHMMSS; a "-N"
    // collision suffix may follow it, a program prefix never precedes it.
    ATOM_CHECK(first_stem.size() >= 15);
    for (std::size_t i = 0; i < 15; ++i) {
        if (i == 8)
            ATOM_CHECK(first_stem[i] == '-');
        else
            ATOM_CHECK(std::isdigit(static_cast<unsigned char>(first_stem[i])) != 0);
    }

    LOG_ERROR(atom::log::core::Main, "file-sink-smoke");
    atom::Log::FlushFile();
    ATOM_CHECK(atom::Log::GetPendingFileLineCount() == 0);
    ATOM_CHECK(read_text(first_path).find("file-sink-smoke") != std::string::npos);

    // Enabling twice is a no-op: it must not truncate or rotate the file.
    atom::Log::SetFileOutput(true);
    ATOM_CHECK(atom::Log::GetFileOutputPath() == first_path);

    // Disabling drains what is still queued instead of dropping it.
    LOG_ERROR(atom::log::core::Main, "file-sink-tail");
    atom::Log::SetFileOutput(false);
    ATOM_CHECK(!atom::Log::IsFileOutputEnabled());
    ATOM_CHECK(atom::Log::GetFileOutputPath().empty());
    ATOM_CHECK(read_text(first_path).find("file-sink-tail") != std::string::npos);

    // The next run opens its own file: the first run's log is never truncated,
    // even when both runs start within the same second.
    atom::Log::SetFileOutput(true);
    ATOM_CHECK(atom::Log::IsFileOutputEnabled());
    const std::string second_path = atom::Log::GetFileOutputPath();
    ATOM_CHECK(!second_path.empty());
    ATOM_CHECK(second_path != first_path);
    LOG_ERROR(atom::log::core::Main, "file-sink-second-run");
    atom::Log::SetFileOutput(false);
    ATOM_CHECK(read_text(first_path).find("file-sink-second-run") == std::string::npos);
    ATOM_CHECK(read_text(second_path).find("file-sink-second-run") != std::string::npos);

    {
        std::error_code cleanup_ec;
        std::filesystem::remove(first_file, cleanup_ec);
        std::filesystem::remove(std::filesystem::path{second_path}, cleanup_ec);
        // Removes Log/ only when this test's files were the only entries in it.
        std::filesystem::remove(first_file.parent_path(), cleanup_ec);
    }

    // Capacity 0 disables buffering entirely.
    atom::Log::SetRecentCapacity(0);
    LOG_INFO(atom::log::core::Main, "not-buffered");
    ATOM_CHECK(atom::Log::GetRecentRecords().empty());
    return 0;
}
