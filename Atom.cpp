#include "Log/LogSystem.hpp"

auto main() -> int {
    atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_INFO);

    LOG_INFO(atom::core::LogChannel::MAIN, R"(
    +=============================================================+
    |                         ATOM ENGINE                         |
    |                                          Beta Insider build |
    |                                                     by romi |
    +=============================================================+)"
    );

    LOG_INFO(atom::core::LogChannel::MAIN, "Atom engine finished up :)");
}
