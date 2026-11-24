#ifndef ATOM_HOMELESS_PROMENADEDEBUGGER_HPP
#define ATOM_HOMELESS_PROMENADEDEBUGGER_HPP

namespace atom::runtime {
class Application;
}

namespace homeless {

auto RunWithDebugger(atom::runtime::Application& app, bool show_debugger, bool jobs_started) -> void;

} // namespace homeless

#endif // ATOM_HOMELESS_PROMENADEDEBUGGER_HPP
