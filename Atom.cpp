#include <iostream>

#include "Log/LogSystem.hpp"

auto main() -> int {
    atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_INFO);

    std::cout << R"(
    +=============================================================+
    |                         ATOM ENGINE                         |
    |                                          Beta Insider build |
    |                                                     by romi |
    +=============================================================+
    )" << std::endl;

    LOG_INFO(atom::LogChannel::ATOM_MAIN, "Atom engine finished up :)");
}
