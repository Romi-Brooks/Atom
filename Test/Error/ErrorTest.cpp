/**
 * @file           : ErrorTest.cpp
 * @brief          : Contract tests for atom::Error / Result.
 *
 * Covers the parts other modules will depend on:
 *   - code/module/operation/resource/detail are preserved
 *   - Message() renders exactly the fields that are present
 *   - With*() annotates without overwriting a lower layer's context
 *   - ModuleOf()/MatchesModule() agree with the code blocks
 *   - platform error capture reads and clears the platform slot
 */

#include <Error/Error.hpp>

#include <Test/Support/TestHelpers.hpp>

#include <cstdint>
#include <string>
#include <type_traits>

#include <SDL3/SDL_error.h>

namespace {

using atom::Error;
using atom::ErrorCode;
using atom::Module;

// Result<T> must stay a value type with the expected payload semantics.
static_assert(std::is_same_v<atom::Result<int>, std::expected<int, Error>>);
static_assert(std::is_same_v<atom::Result<void>, std::expected<void, Error>>);

auto Contains(const std::string& haystack, const std::string& needle) -> bool {
    return haystack.find(needle) != std::string::npos;
}

} // namespace

auto main() -> int {
    // --- 1. Default and explicit construction -----------------------------
    {
        // A default Error must never look like success.
        const Error defaulted{};
        ATOM_CHECK(defaulted.Code() == ErrorCode::Internal);
        ATOM_CHECK(defaulted.GetModule() == Module::Core);
        ATOM_CHECK(defaulted.Operation().empty());
        ATOM_CHECK(defaulted.Detail().empty());

        const Error explicit_error{ErrorCode::FsNotFound, Module::Fs, "OpenRead", "no such entry"};
        ATOM_CHECK(explicit_error.Code() == ErrorCode::FsNotFound);
        ATOM_CHECK(explicit_error.GetModule() == Module::Fs);
        ATOM_CHECK(explicit_error.Operation() == "OpenRead");
        ATOM_CHECK(explicit_error.Detail() == "no such entry");
    }

    // --- 2. Message() renders present fields only -------------------------
    {
        const Error minimal{ErrorCode::AssetNoLoader, Module::Asset};
        const auto minimal_text = minimal.Message();
        ATOM_CHECK(Contains(minimal_text, "module=asset"));
        ATOM_CHECK(Contains(minimal_text, "code=AssetNoLoader"));
        // No operation/resource/detail were set, so they must not appear as
        // empty "op=" fields.
        ATOM_CHECK(!Contains(minimal_text, "op="));
        ATOM_CHECK(!Contains(minimal_text, "resource="));
        ATOM_CHECK(!Contains(minimal_text, "detail="));

        Error full{ErrorCode::FsIoError, Module::Fs, "ReadAt", "short read"};
        full.WithResource("res://textures/ui/button.png");
        const auto full_text = full.Message();
        ATOM_CHECK(Contains(full_text, "module=fs"));
        ATOM_CHECK(Contains(full_text, "code=FsIoError"));
        ATOM_CHECK(Contains(full_text, "op=ReadAt"));
        ATOM_CHECK(Contains(full_text, "resource=res://textures/ui/button.png"));
        ATOM_CHECK(Contains(full_text, "detail=short read"));
    }

    // --- 3. Annotation never overwrites existing context -------------------
    {
        Error error{ErrorCode::AssetNotFound, Module::Asset, "Load"};
        error.WithOperation("Acquire");  // lower layer already named the op
        ATOM_CHECK(error.Operation() == "Load");

        error.WithResource("res://a.bin");
        error.WithResource("res://b.bin");
        ATOM_CHECK(error.Resource() == "res://a.bin");

        error.WithDetail("first");
        error.WithDetail("second");
        ATOM_CHECK(error.Detail() == "first");

        // Chaining is the intended reading order.
        const auto chained =
            Error{ErrorCode::FsInvalidPath, Module::Fs}.WithOperation("TryParse").WithDetail("empty");
        ATOM_CHECK(chained.Operation() == "TryParse");
        ATOM_CHECK(chained.Detail() == "empty");
    }

    // --- 4. Module ownership matches the code blocks ----------------------
    {
        ATOM_CHECK(atom::ModuleOf(ErrorCode::FsNotFound) == Module::Fs);
        ATOM_CHECK(atom::ModuleOf(ErrorCode::AssetLoadFailed) == Module::Asset);
        ATOM_CHECK(atom::ModuleOf(ErrorCode::RenderDeviceCreationFailed) == Module::Render);
        ATOM_CHECK(atom::ModuleOf(ErrorCode::AudioDecoderOpenFailed) == Module::Audio);
        ATOM_CHECK(atom::ModuleOf(ErrorCode::WindowCreationFailed) == Module::Window);
        ATOM_CHECK(atom::ModuleOf(ErrorCode::JobRejected) == Module::Concurrency);
        ATOM_CHECK(atom::ModuleOf(ErrorCode::LuaLoadFailed) == Module::Lua);
        ATOM_CHECK(atom::ModuleOf(ErrorCode::LayoutCycleDetected) == Module::Layout);
        ATOM_CHECK(atom::ModuleOf(ErrorCode::VideoDecodeFailed) == Module::Video);
        ATOM_CHECK(atom::ModuleOf(ErrorCode::ToolInvalidInput) == Module::Tools);
        // Core codes are the fallback, and they must stay in the low block.
        ATOM_CHECK(atom::ModuleOf(ErrorCode::InvalidArgument) == Module::Core);
        ATOM_CHECK(atom::ModuleOf(ErrorCode::Ok) == Module::Core);

        ATOM_CHECK(Error{ErrorCode::FsNotFound, Module::Fs}.MatchesModule());
        // Wrapping a lower layer's failure in the caller's module is legal,
        // and MatchesModule() reports the mismatch instead of hiding it.
        ATOM_CHECK(!Error{ErrorCode::FsNotFound, Module::Asset}.MatchesModule());
    }

    // --- 5. Code and module names are total -------------------------------
    {
        ATOM_CHECK(atom::ErrorCodeName(ErrorCode::Ok) == "Ok");
        ATOM_CHECK(atom::ErrorCodeName(ErrorCode::FsOutsideRoot) == "FsOutsideRoot");
        ATOM_CHECK(atom::ModuleName(Module::Render) == "render");
        ATOM_CHECK(atom::ModuleName(Module::Tools) == "tools");
    }

    // --- 6. Result carries value or error ---------------------------------
    {
        atom::Result<int> good{7};
        ATOM_CHECK(good.has_value());
        ATOM_CHECK(*good == 7);

        atom::Result<int> bad = std::unexpected<Error>{Error{ErrorCode::AssetLoadFailed, Module::Asset}};
        ATOM_CHECK(!bad.has_value());
        ATOM_CHECK(bad.error().Code() == ErrorCode::AssetLoadFailed);

        atom::Result<void> ok = atom::Ok();
        ATOM_CHECK(ok.has_value());

        atom::Result<void> failed = atom::Fail(Error{ErrorCode::RenderFrameRejected, Module::Render});
        ATOM_CHECK(!failed.has_value());
        ATOM_CHECK(failed.error().Code() == ErrorCode::RenderFrameRejected);
    }

    // --- 7. Platform error capture reads and clears the slot --------------
    {
        // Success path: no error recorded means an empty detail, not a stale one.
        SDL_ClearError();
        ATOM_CHECK(atom::CaptureSdlError().empty());

        SDL_SetError("synthetic failure %d", 42);
        const auto captured = atom::CaptureSdlError();
        ATOM_CHECK(captured == "synthetic failure 42");
        // Clearing is what keeps the next reader from re-reporting this failure.
        ATOM_CHECK(atom::CaptureSdlError().empty());
    }

    // --- 8. Errno capture never throws and reports something --------------
    {
        // Not asserting the text (locale/CRT dependent), only that the helper is
        // total: it must always produce a non-empty string.
        const auto message = atom::CaptureErrnoMessage();
        ATOM_CHECK(!message.empty());
    }

    return 0;
}
