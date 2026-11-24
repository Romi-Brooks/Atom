#include "Log/LogSystem.hpp"

auto main() -> int {
    atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_INFO);

    LOG_INFO(atom::log::core::Main, R"(
    +=============================================================+
    |                         ATOM ENGINE                         |
    |                                          Beta Insider build |
    |                                                     by romi |
    +=============================================================+)"
    );

    LOG_INFO(atom::log::core::Main, "Atom engine finished up :)");
}
