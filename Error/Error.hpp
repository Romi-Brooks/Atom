/**
 * @file           : Error.hpp
 * @brief          : The engine's unified error value: a stable code, the module
 *                   that raised it, the operation, the resource involved, and a
 *                   captured detail string.
 *
 * Design rules (CORE-004). Read these before adding a new failure path:
 *
 *  1. A function that can fail returns `atom::Result<T>` (`Result<void>` for
 *     "succeeded or not, with a reason"). Do not add module-specific result
 *     enums; convert the ones that already exist (~ErrorCodeFrom* below) and
 *     let the unified value travel.
 *  2. Add the failure code to the range of the module that owns the decision.
 *     Codes are stable: never renumber an existing enumerator, never reuse a
 *     retired value. Append inside the module's block.
 *  3. Capture the low-level reason where it happens. Platform error state
 *     (SDL_GetError, GetLastError, errno) is overwritten by any later call, so
 *     it must be read at the failure site - FromPlatformError() does that.
 *  4. `Error` never logs. The module that fails decides what to log, so one
 *     failure produces one diagnostic instead of one per layer.
 *  5. `Error` is a plain value: no exception, no allocation on the happy path,
 *     no dependency on Log or any backend.
 *
 * Still logging a failure and returning `void` or `bool`? That is the pattern
 * CORE-004 removes - the caller cannot react to something it cannot see.
**/

#ifndef ATOM_ERROR_ERROR_HPP
#define ATOM_ERROR_ERROR_HPP

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <utility>

namespace atom {

// --- Codes ------------------------------------------------------------------
//
// Each engine module owns a 256-value block, so two modules can add codes
// without coordinating. `Ok` is the only cross-module value: a Result either
// carries an error or no error at all, never "Ok" as a failure.

enum class Module : std::uint8_t {
    Core = 0,
    Fs,
    Asset,
    Render,
    Audio,
    Window,
    Time,
    Concurrency,
    Lua,
    Layout,
    Log,
    Video,
    Tools,
    // Appended, never inserted: the enumerator order is not wire format, but
    // keeping additions at the end makes review diffs trivial.
    Ecs,
};

enum class ErrorCode : std::uint32_t {
    Ok = 0,

    // --- Core (0x0000) ------------------------------------------------------
    InvalidArgument = 0x0001,
    NotInitialized,
    AlreadyInitialized,
    Unsupported,
    OutOfMemory,
    Internal,

    // --- Filesystem (0x0100) ------------------------------------------------
    FsInvalidPath = 0x0100,
    FsNotFound,
    FsNotFile,
    FsNotDirectory,
    FsOutsideRoot,
    FsOutOfRange,
    FsIoError,
    FsInvalidUtf8,
    FsMissingMount,
    FsInvalidMount,
    FsInvalidSeparator,

    // --- Asset (0x0200) -----------------------------------------------------
    AssetInvalidId = 0x0200,
    AssetNoLoader,
    AssetNotFound,
    AssetNotFile,
    AssetIoError,
    AssetTypeMismatch,
    AssetLoadFailed,
    AssetDecoderUnsupported,
    AssetDecoderInvalidData,
    AssetImageDecodeFailed,
    AssetImageFormatUnsupported,

    // --- Render (0x0300) ----------------------------------------------------
    RenderBackendUnavailable = 0x0300,
    RenderDeviceCreationFailed,
    RenderSwapchainUnavailable,
    RenderShaderLoadFailed,
    RenderPipelineCreationFailed,
    RenderTextureCreationFailed,
    RenderSamplerCreationFailed,
    RenderBufferCreationFailed,
    RenderPassBeginFailed,
    RenderCommandSubmitFailed,
    RenderFrameRejected,
    RenderResourceInvalid,

    // --- Audio (0x0400) -----------------------------------------------------
    AudioBackendUnavailable = 0x0400,
    AudioDeviceOpenFailed,
    AudioStreamOpenFailed,
    AudioStreamWriteFailed,
    AudioDecoderOpenFailed,
    AudioDecodeFailed,
    AudioSeekUnsupported,
    AudioTrackNotFound,
    AudioVoiceLimitReached,

    // --- Window (0x0500) ----------------------------------------------------
    WindowCreationFailed = 0x0500,
    WindowNotCreated,
    WindowEventPumpFailed,

    // --- Time (0x0600) ------------------------------------------------------
    TimeSourceUnavailable = 0x0600,
    TimeDomainUnknown,

    // --- Concurrency (0x0700) -----------------------------------------------
    JobSystemNotRunning = 0x0700,
    JobSystemStartFailed,
    JobRejected,
    JobDependencyDropped,

    // --- Lua (0x0800) -------------------------------------------------------
    LuaStateCreationFailed = 0x0800,
    LuaLoadFailed,
    LuaRuntimeError,
    LuaContextMissing,
    LuaStackOverflow,

    // --- Layout (0x0900) ----------------------------------------------------
    LayoutInvalidTree = 0x0900,
    LayoutCycleDetected,
    LayoutOutOfRange,

    // --- Log (0x0A00) -------------------------------------------------------
    LogSinkFailed = 0x0A00,

    // --- Video (0x0B00) -----------------------------------------------------
    VideoDecoderOpenFailed = 0x0B00,
    VideoDecodeFailed,
    VideoUnsupported,

    // --- Tools (0x0C00) -----------------------------------------------------
    ToolInvalidInput = 0x0C00,
    ToolDuplicateEntry,
    ToolVersionUnsupported,
    ToolWriteFailed,
    ToolVerificationFailed,

    // --- ECS (0x0D00) -------------------------------------------------------
    // The component backend reports most misuse with assertions that are
    // disabled in release builds, so these codes exist to turn "the backend
    // asserted, or silently corrupted state" into a value the caller can act on.
    // The ECS facade validates against these before any backend call.
    EcsInvalidEntity = 0x0D00,
    EcsEntityNotAlive,
    EcsComponentNotRegistered,
    EcsComponentAlreadyPresent,
    EcsComponentMissing,
    EcsTypeIdCollision,
    EcsStructuralChangeDuringIteration,
    EcsOutOfEntities,
};

// Canonical name of a module, for logs and diagnostics. Named rather than an
// overload of ToString so that unqualified calls in module namespaces (where
// each module defines its own ToString) cannot become ambiguous.
[[nodiscard]] constexpr auto ModuleName(const Module module) -> std::string_view {
    switch (module) {
        case Module::Core:
            return "core";
        case Module::Fs:
            return "fs";
        case Module::Asset:
            return "asset";
        case Module::Render:
            return "render";
        case Module::Audio:
            return "audio";
        case Module::Window:
            return "window";
        case Module::Time:
            return "time";
        case Module::Concurrency:
            return "concurrency";
        case Module::Lua:
            return "lua";
        case Module::Layout:
            return "layout";
        case Module::Log:
            return "log";
        case Module::Video:
            return "video";
        case Module::Tools:
            return "tools";
        case Module::Ecs:
            return "ecs";
    }
    return "unknown";
}

[[nodiscard]] constexpr auto ErrorCodeName(const ErrorCode code) -> std::string_view {
    switch (code) {
        case ErrorCode::Ok:
            return "Ok";

        case ErrorCode::InvalidArgument:
            return "InvalidArgument";
        case ErrorCode::NotInitialized:
            return "NotInitialized";
        case ErrorCode::AlreadyInitialized:
            return "AlreadyInitialized";
        case ErrorCode::Unsupported:
            return "Unsupported";
        case ErrorCode::OutOfMemory:
            return "OutOfMemory";
        case ErrorCode::Internal:
            return "Internal";

        case ErrorCode::FsInvalidPath:
            return "FsInvalidPath";
        case ErrorCode::FsNotFound:
            return "FsNotFound";
        case ErrorCode::FsNotFile:
            return "FsNotFile";
        case ErrorCode::FsNotDirectory:
            return "FsNotDirectory";
        case ErrorCode::FsOutsideRoot:
            return "FsOutsideRoot";
        case ErrorCode::FsOutOfRange:
            return "FsOutOfRange";
        case ErrorCode::FsIoError:
            return "FsIoError";
        case ErrorCode::FsInvalidUtf8:
            return "FsInvalidUtf8";
        case ErrorCode::FsMissingMount:
            return "FsMissingMount";
        case ErrorCode::FsInvalidMount:
            return "FsInvalidMount";
        case ErrorCode::FsInvalidSeparator:
            return "FsInvalidSeparator";

        case ErrorCode::AssetInvalidId:
            return "AssetInvalidId";
        case ErrorCode::AssetNoLoader:
            return "AssetNoLoader";
        case ErrorCode::AssetNotFound:
            return "AssetNotFound";
        case ErrorCode::AssetNotFile:
            return "AssetNotFile";
        case ErrorCode::AssetIoError:
            return "AssetIoError";
        case ErrorCode::AssetTypeMismatch:
            return "AssetTypeMismatch";
        case ErrorCode::AssetLoadFailed:
            return "AssetLoadFailed";
        case ErrorCode::AssetDecoderUnsupported:
            return "AssetDecoderUnsupported";
        case ErrorCode::AssetDecoderInvalidData:
            return "AssetDecoderInvalidData";
        case ErrorCode::AssetImageDecodeFailed:
            return "AssetImageDecodeFailed";
        case ErrorCode::AssetImageFormatUnsupported:
            return "AssetImageFormatUnsupported";

        case ErrorCode::RenderBackendUnavailable:
            return "RenderBackendUnavailable";
        case ErrorCode::RenderDeviceCreationFailed:
            return "RenderDeviceCreationFailed";
        case ErrorCode::RenderSwapchainUnavailable:
            return "RenderSwapchainUnavailable";
        case ErrorCode::RenderShaderLoadFailed:
            return "RenderShaderLoadFailed";
        case ErrorCode::RenderPipelineCreationFailed:
            return "RenderPipelineCreationFailed";
        case ErrorCode::RenderTextureCreationFailed:
            return "RenderTextureCreationFailed";
        case ErrorCode::RenderSamplerCreationFailed:
            return "RenderSamplerCreationFailed";
        case ErrorCode::RenderBufferCreationFailed:
            return "RenderBufferCreationFailed";
        case ErrorCode::RenderPassBeginFailed:
            return "RenderPassBeginFailed";
        case ErrorCode::RenderCommandSubmitFailed:
            return "RenderCommandSubmitFailed";
        case ErrorCode::RenderFrameRejected:
            return "RenderFrameRejected";
        case ErrorCode::RenderResourceInvalid:
            return "RenderResourceInvalid";

        case ErrorCode::AudioBackendUnavailable:
            return "AudioBackendUnavailable";
        case ErrorCode::AudioDeviceOpenFailed:
            return "AudioDeviceOpenFailed";
        case ErrorCode::AudioStreamOpenFailed:
            return "AudioStreamOpenFailed";
        case ErrorCode::AudioStreamWriteFailed:
            return "AudioStreamWriteFailed";
        case ErrorCode::AudioDecoderOpenFailed:
            return "AudioDecoderOpenFailed";
        case ErrorCode::AudioDecodeFailed:
            return "AudioDecodeFailed";
        case ErrorCode::AudioSeekUnsupported:
            return "AudioSeekUnsupported";
        case ErrorCode::AudioTrackNotFound:
            return "AudioTrackNotFound";
        case ErrorCode::AudioVoiceLimitReached:
            return "AudioVoiceLimitReached";

        case ErrorCode::WindowCreationFailed:
            return "WindowCreationFailed";
        case ErrorCode::WindowNotCreated:
            return "WindowNotCreated";
        case ErrorCode::WindowEventPumpFailed:
            return "WindowEventPumpFailed";

        case ErrorCode::TimeSourceUnavailable:
            return "TimeSourceUnavailable";
        case ErrorCode::TimeDomainUnknown:
            return "TimeDomainUnknown";

        case ErrorCode::JobSystemNotRunning:
            return "JobSystemNotRunning";
        case ErrorCode::JobSystemStartFailed:
            return "JobSystemStartFailed";
        case ErrorCode::JobRejected:
            return "JobRejected";
        case ErrorCode::JobDependencyDropped:
            return "JobDependencyDropped";

        case ErrorCode::LuaStateCreationFailed:
            return "LuaStateCreationFailed";
        case ErrorCode::LuaLoadFailed:
            return "LuaLoadFailed";
        case ErrorCode::LuaRuntimeError:
            return "LuaRuntimeError";
        case ErrorCode::LuaContextMissing:
            return "LuaContextMissing";
        case ErrorCode::LuaStackOverflow:
            return "LuaStackOverflow";

        case ErrorCode::LayoutInvalidTree:
            return "LayoutInvalidTree";
        case ErrorCode::LayoutCycleDetected:
            return "LayoutCycleDetected";
        case ErrorCode::LayoutOutOfRange:
            return "LayoutOutOfRange";

        case ErrorCode::LogSinkFailed:
            return "LogSinkFailed";

        case ErrorCode::VideoDecoderOpenFailed:
            return "VideoDecoderOpenFailed";
        case ErrorCode::VideoDecodeFailed:
            return "VideoDecodeFailed";
        case ErrorCode::VideoUnsupported:
            return "VideoUnsupported";

        case ErrorCode::ToolInvalidInput:
            return "ToolInvalidInput";
        case ErrorCode::ToolDuplicateEntry:
            return "ToolDuplicateEntry";
        case ErrorCode::ToolVersionUnsupported:
            return "ToolVersionUnsupported";
        case ErrorCode::ToolWriteFailed:
            return "ToolWriteFailed";
        case ErrorCode::ToolVerificationFailed:
            return "ToolVerificationFailed";

        case ErrorCode::EcsInvalidEntity:
            return "EcsInvalidEntity";
        case ErrorCode::EcsEntityNotAlive:
            return "EcsEntityNotAlive";
        case ErrorCode::EcsComponentNotRegistered:
            return "EcsComponentNotRegistered";
        case ErrorCode::EcsComponentAlreadyPresent:
            return "EcsComponentAlreadyPresent";
        case ErrorCode::EcsComponentMissing:
            return "EcsComponentMissing";
        case ErrorCode::EcsTypeIdCollision:
            return "EcsTypeIdCollision";
        case ErrorCode::EcsStructuralChangeDuringIteration:
            return "EcsStructuralChangeDuringIteration";
        case ErrorCode::EcsOutOfEntities:
            return "EcsOutOfEntities";
    }
    return "UnknownErrorCode";
}

// --- The error value --------------------------------------------------------

class Error {
    public:
        // An Error is never "no error": Result carries the presence, so a
        // default-constructed Error still names a code (Internal) instead of
        // pretending to be Ok.
        Error() = default;

        Error(const ErrorCode code, const Module module, std::string operation = {},
              std::string detail = {})
            : code_(code), module_(module), operation_(std::move(operation)), detail_(std::move(detail)) {}

        [[nodiscard]] auto Code() const noexcept -> ErrorCode {
            return code_;
        }
        [[nodiscard]] auto GetModule() const noexcept -> Module {
            return module_;
        }
        [[nodiscard]] auto Operation() const noexcept -> std::string_view {
            return operation_;
        }
        [[nodiscard]] auto Detail() const noexcept -> std::string_view {
            return detail_;
        }

        // The resource the failing operation was applied to, as an AssetPath
        // string. Empty when the operation is not resource-bound. Kept as a
        // string so Error does not depend on the Filesystem module.
        [[nodiscard]] auto Resource() const noexcept -> std::string_view {
            return resource_;
        }

        // Attaches context in place and returns *this, so a failure can be
        // annotated while it propagates:
        //   return ToError(result, "OpenRead").WithResource(path.String());
        // The first annotation wins: a lower layer usually knows more than the
        // layer re-reporting it, so later calls do not overwrite.
        auto WithOperation(std::string operation) -> Error& {
            if (operation_.empty())
                operation_ = std::move(operation);
            return *this;
        }

        auto WithResource(std::string_view resource) -> Error& {
            if (resource_.empty())
                resource_ = std::string{resource};
            return *this;
        }

        auto WithDetail(std::string detail) -> Error& {
            if (detail_.empty())
                detail_ = std::move(detail);
            return *this;
        }

        // "module=render code=RenderDeviceCreationFailed op=Initialize
        //  resource=res://x detail=SDL_CreateGPUDevice failed: ..."
        // Fields that are empty are omitted.
        [[nodiscard]] auto Message() const -> std::string;

        // True when the code belongs to the module's own block. Diagnostics
        // only - Error does not enforce it, so wrapping a lower layer's code
        // (fs failure surfacing from asset) stays legal and visible.
        [[nodiscard]] auto MatchesModule() const noexcept -> bool;

    private:
        ErrorCode code_ = ErrorCode::Internal;
        Module module_ = Module::Core;
        std::string operation_{};
        std::string resource_{};
        std::string detail_{};
};

// --- Platform error capture -------------------------------------------------
//
// Platform error state is a single slot that the next API call overwrites
// (SDL_GetError, GetLastError, errno), so it must be copied at the failure site
// rather than read later by an upper layer. These live in the implementation
// file so Error.hpp stays free of platform headers.

// Copies SDL_GetError() and clears it. Returns an empty string when SDL has no
// error recorded.
[[nodiscard]] auto CaptureSdlError() -> std::string;

// Copies and clears GetLastError(), formatted by FormatMessage.
[[nodiscard]] auto CaptureLastSystemError() -> std::string;

// Copies errno's message. Does not clear errno; callers that need that must do
// it themselves right after the failing call.
[[nodiscard]] auto CaptureErrnoMessage() -> std::string;

// --- Construction helpers ---------------------------------------------------

[[nodiscard]] inline auto MakeError(const ErrorCode code, const Module module,
                                    std::string_view operation = {}) -> Error {
    return Error{code, module, std::string{operation}};
}

// The module that owns a code, derived from its block. Useful when a helper
// raises a code on behalf of another module.
[[nodiscard]] constexpr auto ModuleOf(const ErrorCode code) -> Module {
    switch (static_cast<std::uint32_t>(code) & 0xFF00U) {
        case 0x0100:
            return Module::Fs;
        case 0x0200:
            return Module::Asset;
        case 0x0300:
            return Module::Render;
        case 0x0400:
            return Module::Audio;
        case 0x0500:
            return Module::Window;
        case 0x0600:
            return Module::Time;
        case 0x0700:
            return Module::Concurrency;
        case 0x0800:
            return Module::Lua;
        case 0x0900:
            return Module::Layout;
        case 0x0A00:
            return Module::Log;
        case 0x0B00:
            return Module::Video;
        case 0x0C00:
            return Module::Tools;
        case 0x0D00:
            return Module::Ecs;
        default:
            return Module::Core;
    }
}

// --- Result -----------------------------------------------------------------

// The engine's failure-carrying return value. `Result<void>` is the "returns
// nothing, but can fail" form:
//     [[nodiscard]] auto Shutdown() -> Result<void>;
//     if (!result) { LOG_ERROR(channel, result.error().Message()); }
//
// Note that [[nodiscard]] does not propagate from the return type to the
// Result object, so functions returning it should still be marked
// [[nodiscard]] themselves - the coding standard requires that for every
// function whose return value must not be ignored.
template <typename T> using Result = std::expected<T, Error>;

// Replaces a plain `bool` success flag with "success or a reason".
[[nodiscard]] inline auto Ok() -> Result<void> {
    return Result<void>{};
}

[[nodiscard]] inline auto Fail(Error error) -> Result<void> {
    return std::unexpected<Error>{std::move(error)};
}

} // namespace atom

#endif // ATOM_ERROR_ERROR_HPP
