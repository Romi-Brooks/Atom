#pragma once

#include <functional>
#include <optional>
#include <string>

#include <Error/Error.hpp>

namespace atom::platform {

enum class PathDialog { OpenFile, SaveFile, OpenFolder };

// Call on the window thread. The callback may run on another thread. An empty
// optional means cancellation, and a returned path is encoded as UTF-8.
using PathDialogCallback = std::function<void(Result<std::optional<std::string>>)>;
auto ShowPathDialog(PathDialog mode, PathDialogCallback callback) -> void;

} // namespace atom::platform
