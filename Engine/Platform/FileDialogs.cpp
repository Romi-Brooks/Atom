#include "FileDialogs.hpp"

#include <memory>

#include <SDL3/SDL_dialog.h>

#include <Backend/SDL3/Core/SdlError.hpp>

namespace atom::platform {
namespace {

void SDLCALL CompleteDialog(void* context, const char* const* paths, int) {
    std::unique_ptr<PathDialogCallback> callback{static_cast<PathDialogCallback*>(context)};
    if (!paths) {
        auto error = MakeError(ErrorCode::FsIoError, Module::Fs, "ShowPathDialog");
        error.WithDetail(backend::sdl3::CaptureError());
        (*callback)(std::unexpected{std::move(error)});
    } else if (*paths) {
        (*callback)(std::optional<std::string>{*paths});
    } else {
        (*callback)(std::nullopt);
    }
}

} // namespace

auto ShowPathDialog(const PathDialog mode, PathDialogCallback callback) -> void {
    auto* context = new PathDialogCallback(std::move(callback));
    switch (mode) {
        case PathDialog::OpenFile:
            SDL_ShowOpenFileDialog(CompleteDialog, context, nullptr, nullptr, 0, nullptr, false);
            break;
        case PathDialog::SaveFile:
            SDL_ShowSaveFileDialog(CompleteDialog, context, nullptr, nullptr, 0, nullptr);
            break;
        case PathDialog::OpenFolder:
            SDL_ShowOpenFolderDialog(CompleteDialog, context, nullptr, nullptr, false);
            break;
    }
}

} // namespace atom::platform
