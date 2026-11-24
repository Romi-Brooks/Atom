/**
  * @file           : IRenderDevice.hpp
  * @author         : Romi Brooks
  * @brief          : Backend-neutral render device capability contract.
  * @attention      :
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_BACKEND_CONTRACTS_RENDER_IRENDERDEVICE_HPP
#define ATOM_BACKEND_CONTRACTS_RENDER_IRENDERDEVICE_HPP

#include <cstdint>
#include <string>

#include <Algorithm/Vector/Vec2.hpp>
#include <Backend/Contracts/Render/IRenderTypes.hpp>
#include <Backend/Contracts/Render/PresentMode.hpp>

namespace atom::render {

// Per-frame timing, filled by the device as it runs the frame. Keeping this on
// the device rather than deriving it in tooling matters, because only the
// device knows how much of a frame was spent *blocked on the GPU*: that number
// is the difference between "the CPU cannot submit any faster" and "the GPU is
// behind", which is the first question any frame-rate investigation asks.
//
// There is deliberately no `gpu_busy_ms`: SDL_GPU exposes no timestamp queries
// (no SDL_GPUTimestampQuery in the shipped version), so a backend cannot know
// how long its shaders ran. gpu_period_ms is the honest substitute — the
// interval between successive GPU fence completions, which equals the frame
// period whenever the GPU is the limiter.
struct FrameTiming {
        // Wall time since the previous BeginFrame(): the device-side frame
        // period, independent of any tool's own sampling.
        double frame_ms = 0.0;
        // Blocked inside BeginFrame() waiting for the frame that used this
        // in-flight slot two frames ago. Non-zero means the GPU (or the
        // presentation queue) is applying backpressure right now.
        double cpu_wait_ms = 0.0;
        // BeginFrame() end -> end of recording: batching, uploads, command
        // encoding. This is the engine's own per-frame CPU cost.
        double cpu_encode_ms = 0.0;
        // Time inside the submit call itself, which is also where the swapchain
        // is presented. A backend whose presentation waits for the vertical
        // blank blocks here, not in the fence wait above; keeping the two apart
        // is what distinguishes "our submit path is slow" from "the driver is
        // holding us at the refresh rate".
        double cpu_submit_ms = 0.0;
        // Interval between the last two fence completions, i.e. how fast frames
        // actually retire on the GPU. Only meaningful while the CPU is waiting
        // on fences (gpu_period is otherwise a lower bound sampled at submit
        // rate, not the GPU's own pace).
        double gpu_period_ms = 0.0;
};

// Optional per-frame accounting of where the record window went, so a tool can
// attribute a slow frame without a profiler. Backends fill what they can measure
// and leave the rest at zero; the same fields mean the same thing everywhere
// (all values are totals over GetEncoderPhaseFrames() frames, not averages).
//
// It exists because a backend can make one phase arbitrarily more expensive than
// another for an identical command stream: SDL_GPU's Vulkan path measured 34x
// the D3D12 record time on one frame, and only a per-phase split found which call
// was responsible.
struct EncoderPhases {
        double encoder_upload_ms = 0.0;  // staging map + copy passes
        double encoder_pass_ms = 0.0;    // BeginGPURenderPass .. first bind
        double encoder_items_ms = 0.0;   // per-item bind/scissor/draw loop
        double postprocess_ms = 0.0;     // post-process resolve
        double overlay_upload_ms = 0.0;  // overlay (UI) vertex/index upload
        double overlay_pass_ms = 0.0;    // overlay Begin/EndGPURenderPass only
        double overlay_draw_ms = 0.0;    // overlay draw-data rendering
        double overlay_build_ms = 0.0;   // build the overlay's draw data
        std::uint64_t segments = 0;      // 2D encoder submissions
        std::uint64_t items = 0;         // draw items across those submissions
        std::uint64_t overlay_vertices = 0;
        std::uint64_t postprocess_targets = 0; // target texture creations
        // Texture churn: on Vulkan a release is vkDestroyImage + vkFreeMemory,
        // which can idle the queue, so a resource recreated per frame looks like
        // "slow encoding" while never showing up in any encoder phase.
        std::uint64_t texture_creates = 0;
        std::uint64_t texture_destroys = 0;
};

struct RenderBackendInfo {
        std::string api;
        std::string device;
        // Self-reported name of the system driver ("NVIDIA", "Mesa 23.1.0", ...).
        // Backends cannot always learn it: SDL_GPU fills it on the Vulkan path
        // only, because D3D12 exposes no equivalent of
        // VkPhysicalDeviceDriverProperties. Empty therefore means "this driver
        // does not report a name", not "unknown driver" -- callers should show
        // the absence rather than invent a value.
        std::string driver;
        // Self-reported driver version, in whatever format the driver uses
        // ("32.0.15.6103" for an NVIDIA UMD on D3D12). Available on more drivers
        // than `driver`, so prefer it for support/bug reports.
        std::string driver_version;
        uint32_t shader_formats = 0;
        uint32_t swapchain_format = 0;
};

class IRenderDevice {
    public:
        virtual ~IRenderDevice() = default;
        [[nodiscard]] virtual auto BeginFrame() -> bool = 0;
        virtual auto Clear(const Color& color = Color::Black()) -> void = 0;
        virtual auto EndFrame() -> void = 0;
        virtual auto HandleResize(uint32_t width, uint32_t height) -> void = 0;

        // Swapchain presentation. SetPresentMode() may recreate the swapchain,
        // so it is only valid between frames (after EndFrame, before the next
        // BeginFrame). Returns false when the mode is unsupported or rejected;
        // the applied mode is then unchanged, and GetPresentMode() keeps
        // reporting the old one.
        //
        // PresentMode is the application's request (an in-game "vertical sync"
        // option maps onto it). A GPU control-panel override is invisible from
        // here, so "Immediate" means "asked for, accepted", never "guaranteed
        // uncapped"; see PresentMode.hpp.
        virtual auto SetPresentMode(PresentMode mode) -> bool = 0;
        // The mode the device actually applied (never a request that failed).
        [[nodiscard]] virtual auto GetPresentMode() const -> PresentMode = 0;
        // Bitmask of the modes this device/window/driver accepts, so settings
        // UI can disable the rest instead of probing by failure.
        //
        // NOT a cheap accessor on every backend: SDL_GPU's Vulkan implementation
        // re-queries the surface (capabilities + formats + present modes, plus two
        // allocations) on each call. Measured at ~2.3 ms per query, which turned a
        // per-frame UI call into 6.9 ms/frame and made Vulkan look 34x slower than
        // D3D12 (whose version is a constant). Backends must cache; callers should
        // treat this as a change-driven query, not a per-frame one.
        [[nodiscard]] virtual auto SupportedPresentModes() const -> std::uint8_t = 0;

        // Compatibility shim for the old two-state API, kept so existing call
        // sites and screens keep compiling. Prefer SetPresentMode(): "off"
        // collapses Mailbox and Immediate into one answer, which is exactly the
        // ambiguity the three-mode API exists to remove.
        virtual auto SetVSync(const bool enabled) -> bool {
            return SetPresentMode(enabled ? PresentMode::VSync : PresentMode::Immediate);
        }
        // True when presents are paced to the display refresh, i.e. VSync or
        // Mailbox. Not "mode == VSync": Mailbox is paced too.
        [[nodiscard]] virtual auto IsVSyncEnabled() const -> bool {
            return IsRefreshPaced(GetPresentMode());
        }
        [[nodiscard]] virtual auto GetOutputSize() const -> algo::Vec2 = 0;
        [[nodiscard]] virtual auto GetBackendInfo() const -> const RenderBackendInfo& = 0;
        // Latest frame timing. Non-pure so platforms that cannot measure it
        // (null/mock devices in tests) need not implement it.
        [[nodiscard]] virtual auto GetFrameTiming() const -> FrameTiming {
            return {};
        }
        // Optional per-phase diagnostics for the record window; `{}` means "this
        // backend does not measure them". Pair with GetEncoderPhaseFrames() to
        // turn the totals into per-frame averages.
        [[nodiscard]] virtual auto GetEncoderPhases() const -> EncoderPhases {
            return {};
        }
        [[nodiscard]] virtual auto GetEncoderPhaseFrames() const -> int {
            return 0;
        }
        // Whether the applied present mode really caps the frame rate at the
        // display refresh *on this backend*. Not derivable from PresentMode
        // alone: SDL_GPU's D3D12 path implements Mailbox as an uncapped present
        // (sync interval 0, no tear flag) and lets the desktop compositor drop
        // the surplus, while the Vulkan path maps it onto
        // VK_PRESENT_MODE_MAILBOX_KHR, which does wait for the vertical blank.
        // UI must ask the device instead of inferring from the enum.
        [[nodiscard]] virtual auto IsPresentRefreshPaced() const -> bool {
            return IsRefreshPaced(GetPresentMode());
        }
};

} // namespace atom::render

#endif // ATOM_BACKEND_CONTRACTS_RENDER_IRENDERDEVICE_HPP
