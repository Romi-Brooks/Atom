#include <Log/LogSystem.hpp>
#include <Test/Support/TestHelpers.hpp>

#include <vector>

auto main() -> int {
    auto& log = atom::Log::GetLogInstance();

    const auto channels = atom::Log::GetRegisteredChannels();
    ATOM_CHECK(!channels.empty());
    bool saw_core = false;
    for (const auto& channel : channels) {
        if (channel.name == "Screen.Manager" || channel.name.find("Screen") != std::string::npos)
            saw_core = true;
    }
    ATOM_CHECK(saw_core);

    std::vector<atom::LogRecord> records{};
    auto connection = atom::Log::Subscribe([&records](const atom::LogRecord& record) { records.push_back(record); });
    ATOM_CHECK(connection.IsConnected());

    // Raise console threshold so test output stays quiet; listeners still receive all levels.
    atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_ERROR);

    LOG_INFO(atom::log::core::Main, "atom-log-info-ping");
    LOG_ERROR(atom::log::core::Main, "atom-log-error-ping");
    ATOM_CHECK(records.size() >= 2);

    bool saw_info = false;
    bool saw_error = false;
    for (const auto& record : records) {
        if (record.message == "atom-log-info-ping") {
            saw_info = true;
            ATOM_CHECK(record.level == atom::LogLevel::ATOM_INFO);
            ATOM_CHECK(!record.channel_name.empty());
        }
        if (record.message == "atom-log-error-ping") {
            saw_error = true;
            ATOM_CHECK(record.level == atom::LogLevel::ATOM_ERROR);
        }
    }
    ATOM_CHECK(saw_info);
    ATOM_CHECK(saw_error);

    connection.Reset();
    ATOM_CHECK(!connection.IsConnected());
    const auto count_after_reset = records.size();
    LOG_INFO(atom::log::core::Main, "after-disconnect");
    ATOM_CHECK(records.size() == count_after_reset);

    auto empty = atom::Log::Subscribe({});
    empty.Reset();
    ATOM_CHECK(!empty.IsConnected());
    return 0;
}
