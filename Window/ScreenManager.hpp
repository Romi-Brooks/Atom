/**
  * @file           : ScreenManager.hpp
  * @author         : Romi Brooks
  * @brief          : Screen registration, switching, and per-frame dispatch.
  * @attention      : Registrations are owned through std::shared_ptr and the
  *                   active/stack slots hold the same handles, so replacing or
  *                   unloading a screen can never leave a dangling pointer
  *                   (ARCH-105).
  * @date           : 2025/9/23
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_SCREENMANAGER_HPP
#define ATOM_SCREENMANAGER_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <Window/Screen.hpp>

namespace atom {

// Outcome of every ScreenManager mutation. Returning a value instead of a void
// "did nothing" keeps 未注册 / 空指针 / 同名冲突 distinguishable at the call
// site (ARCH-105 acceptance: every operation result is decidable).
enum class ScreenOpResult : std::uint8_t {
    Success,        // the operation completed
    NotFound,       // the name is not registered
    NullScreen,     // a null Screen* / null unique_ptr was passed
    InvalidName,    // an empty registration name was passed
    NameConflict,   // LoadScreen(Reject) hit an existing registration
    Unregistered,   // the Screen* is not owned by this manager
    InUse,          // the screen is current and cannot be dropped
    EmptyStack,     // PopScreen found nothing to pop
    AlreadyVisible, // Switch/Push targeted the screen that is already current
};

// Human-readable name for logs and tests.
[[nodiscard]] auto ToString(ScreenOpResult result) -> std::string_view;

// What LoadScreen does when `name` is already registered.
enum class ScreenReplacePolicy : std::uint8_t {
    // Keep the registered screen and report NameConflict. This is the default:
    // the silent in-place overwrite is exactly what used to leave
    // current_screen_ / screen_stack_ pointing at a destroyed object.
    Reject,
    // Replace the registration. Because active/stack slots hold shared_ptr
    // handles, they are re-pointed at the new screen before the old one is
    // released; if the replaced screen was current it is deactivated first and
    // the replacement is activated, so activation stays balanced.
    Replace,
};

// Non-owning view of one screen-stack slot, bottom first. Returned by value so
// the manager keeps sole ownership of the underlying handles.
struct ScreenStackEntry {
        std::string name;
        Screen* screen = nullptr;
};

class ScreenManager {
    private:
        // shared_ptr, not unique_ptr: current_screen_ / screen_stack_ are
        // handles into this registry, and a reload must not invalidate them.
        std::unordered_map<std::string, std::shared_ptr<Screen>> screens_{};
        std::shared_ptr<Screen> current_screen_{};
        std::string current_screen_name_{};
        std::vector<std::pair<std::string, std::shared_ptr<Screen>>> screen_stack_{};

        ScreenManager() = default;
        ~ScreenManager() = default;

        // Drops `screen` from every stack slot (used when it is replaced or
        // unloaded) so no slot can outlive its registration.
        auto DetachFromStack(const std::shared_ptr<Screen>& screen) -> void;

    public:
        ScreenManager(const ScreenManager&) = delete;
        ScreenManager& operator=(const ScreenManager&) = delete;

        [[nodiscard]] static auto GetInstance() -> ScreenManager&;

        // Registers (or, with an explicit Replace policy, re-registers) a screen
        // under `name`. The manager takes ownership of `screen` *on entry*: when
        // the call returns NullScreen / InvalidName / NameConflict the screen has
        // already been destroyed, so never hold a raw pointer captured before the
        // call — read it back with GetScreen() after a Success.
        auto LoadScreen(const std::string& name, std::unique_ptr<Screen> screen,
                        ScreenReplacePolicy policy = ScreenReplacePolicy::Reject) -> ScreenOpResult;

        // Removes a registration. Refuses with InUse while the screen is
        // current; stack slots that reference it are dropped first, so the
        // registration and every handle disappear together.
        auto UnloadScreen(const std::string& name) -> ScreenOpResult;

        // Non-owning access to a registered screen. The pointer stays valid for
        // as long as the manager keeps that registration; it is *invalidated* by
        // UnloadScreen(name) and by LoadScreen(name, ..., Replace), both of which
        // destroy the old object once no slot references it. Re-read it after any
        // such call instead of caching it across one.
        [[nodiscard]] auto GetScreen(const std::string& name) const -> Screen*;
        [[nodiscard]] auto GetScreenName(const Screen* screen) const -> std::string;
        // The active screen, or nullptr when nothing has been switched to yet.
        [[nodiscard]] auto GetCurrentScreen() const -> Screen*;

        // Selecting the screen that is already current with an empty stack is a
        // no-op and reports AlreadyVisible. When the stack is not empty the
        // switch is real: the caller asked for a clean slate, so the stack is
        // cleared and the screen is deactivated and activated again (Success).
        auto SwitchScreen(const std::string& name) -> ScreenOpResult;
        auto SwitchScreen(Screen* screen) -> ScreenOpResult;
        // Pushing the screen that is already current is a no-op
        // (AlreadyVisible); nothing is pushed onto the stack twice.
        auto PushScreen(const std::string& name) -> ScreenOpResult;
        auto PushScreen(Screen* screen) -> ScreenOpResult;
        // Restores the top of the stack; EmptyStack when there is nothing to pop.
        auto PopScreen() -> ScreenOpResult;

        auto Render(atom::render::IRenderDevice& device) const -> void;
        auto HandleEvent(const atom::window::IEvent& event) const -> void;
        auto Update(float deltaTime) const -> void;
        auto FixedUpdate(float deltaTime) const -> void;

        [[nodiscard]] auto GetCurrentScreenName() const -> const std::string&;
        [[nodiscard]] auto GetScreenStack() const -> std::vector<ScreenStackEntry>;

        // True when `screen` is current or on the screen stack (the same set
        // ScreenManager::Render draws). Used by screen-scoped debug panels.
        [[nodiscard]] auto IsScreenVisible(const Screen* screen) const -> bool;
};
} // namespace atom
#endif // ATOM_SCREENMANAGER_HPP
