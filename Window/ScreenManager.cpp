/**
  * @file           : ScreenManager.cpp
  * @author         : Romi Brooks
  * @brief          : Screen registration, switching, and per-frame dispatch.
  * @attention      :
  * @date           : 2025/9/23
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#include "ScreenManager.hpp"

#include <ranges>

#include <Log/LogSystem.hpp>

namespace atom {

auto ToString(const ScreenOpResult result) -> std::string_view {
    switch (result) {
    case ScreenOpResult::Success:
        return "Success";
    case ScreenOpResult::NotFound:
        return "NotFound";
    case ScreenOpResult::NullScreen:
        return "NullScreen";
    case ScreenOpResult::InvalidName:
        return "InvalidName";
    case ScreenOpResult::NameConflict:
        return "NameConflict";
    case ScreenOpResult::Unregistered:
        return "Unregistered";
    case ScreenOpResult::InUse:
        return "InUse";
    case ScreenOpResult::EmptyStack:
        return "EmptyStack";
    case ScreenOpResult::AlreadyVisible:
        return "AlreadyVisible";
    }
    return "Unknown";
}

auto ScreenManager::GetInstance() -> ScreenManager& {
    static ScreenManager instance;
    return instance;
}

auto ScreenManager::DetachFromStack(const std::shared_ptr<Screen>& screen) -> void {
    std::erase_if(screen_stack_, [&screen](const auto& entry) { return entry.second == screen; });
}

auto ScreenManager::LoadScreen(const std::string& name, std::unique_ptr<Screen> screen,
                               const ScreenReplacePolicy policy) -> ScreenOpResult {
    if (!screen)
        return ScreenOpResult::NullScreen;
    if (name.empty())
        return ScreenOpResult::InvalidName;

    // One owner from here on: every registry/stack slot holds a shared_ptr, so a
    // reload can never invalidate an outstanding handle.
    std::shared_ptr<Screen> owned{std::move(screen)};

    if (const auto it = screens_.find(name); it != screens_.end() && it->second) {
        if (policy == ScreenReplacePolicy::Reject) {
            // `owned` is released here, so the rejected screen is destroyed: that
            // is why the header warns callers not to keep a pre-call raw pointer.
            LOG_WARNING(atom::log::core::ScreenManager,
                        "LoadScreen refused to replace the registered screen '" + name + "' (NameConflict)");
            return ScreenOpResult::NameConflict;
        }

        // Safe replacement. Everything the map iterator is needed for happens
        // *before* the first user callback: OnDeactivate() may re-enter the
        // manager (Load/Unload/Switch), which can rehash `screens_` and
        // invalidate `it`.
        //
        //  1. every handle is re-pointed at the replacement, so nothing can
        //     observe a dangling pointer or a half-updated registry;
        //  2. the old registration is dropped from the map (still kept alive by
        //     the local `previous`);
        //  3. only then do the lifecycle callbacks run.
        const bool was_current = current_screen_ == it->second;
        auto previous = it->second;
        for (auto& entry : screen_stack_) {
            if (entry.second == previous)
                entry.second = owned;
        }
        it->second = owned;

        if (was_current)
            previous->OnDeactivate();
        if (was_current && current_screen_ == previous) {
            // If the callback switched elsewhere, that choice wins.
            current_screen_ = owned;
            owned->OnActivate();
        }

        // Drop the manager's last handle to the replaced screen now, so the
        // object is destroyed deterministically instead of lingering until this
        // scope ends.
        previous.reset();
        LOG_INFO(atom::log::core::ScreenManager,
                 "Replaced screen '" + name + "'" + (was_current ? " (was active, re-activated)" : ""));
        return ScreenOpResult::Success;
    }

    screens_[name] = std::move(owned);
    LOG_INFO(atom::log::core::ScreenManager, "Registering screen: " + name);
    return ScreenOpResult::Success;
}

auto ScreenManager::UnloadScreen(const std::string& name) -> ScreenOpResult {
    const auto it = screens_.find(name);
    if (it == screens_.end() || !it->second)
        return ScreenOpResult::NotFound;
    if (current_screen_ == it->second) {
        LOG_WARNING(atom::log::core::ScreenManager,
                    "UnloadScreen refused while '" + name + "' is the active screen; switch away first");
        return ScreenOpResult::InUse;
    }

    // Move the handle out and erase the slot before releasing it: a ~Screen that
    // calls back into the manager must not run inside std::unordered_map::erase.
    auto doomed = std::move(it->second);
    // Stack slots hold shared_ptr handles; dropping them here keeps the pop
    // chain from resurrecting an unregistered screen after the erase.
    DetachFromStack(doomed);
    screens_.erase(it);
    doomed.reset();
    LOG_INFO(atom::log::core::ScreenManager, "Unloaded screen: " + name);
    return ScreenOpResult::Success;
}

auto ScreenManager::GetScreen(const std::string& name) const -> Screen* {
    const auto it = screens_.find(name);
    return it != screens_.end() ? it->second.get() : nullptr;
}

auto ScreenManager::GetScreenName(const Screen* screen) const -> std::string {
    if (screen == nullptr)
        return {};
    // Invariant: every live handle (registry slot, current_screen_, every stack
    // slot) refers to a screen that is *registered*. Replace re-points all slots
    // and UnloadScreen detaches from the stack before erasing, so the registry is
    // the single source of truth here.
    for (const auto& [name, owned] : screens_) {
        if (owned.get() == screen)
            return name;
    }
    return {};
}

auto ScreenManager::SwitchScreen(const std::string& name) -> ScreenOpResult {
    const auto it = screens_.find(name);
    if (it == screens_.end() || !it->second) {
        LOG_WARNING(atom::log::core::ScreenManager, "SwitchScreen called with an unregistered screen: " + name);
        return ScreenOpResult::NotFound;
    }
    // Copy the handle out: OnDeactivate() may re-enter the manager and rehash the
    // registry, so `it` must not be used once a callback has run.
    const auto target = it->second;

    if (current_screen_ == target && screen_stack_.empty()) {
        LOG_DEBUG(atom::log::core::ScreenManager, "SwitchScreen ignored: '" + name + "' is already active");
        return ScreenOpResult::AlreadyVisible;
    }

    auto previous = current_screen_;
    if (previous)
        previous->OnDeactivate();
    // Clear after the callback: the stack holds shared_ptr handles, and a
    // re-entrant push/pop must not leave a slot referring to a dropped screen.
    screen_stack_.clear();
    current_screen_ = target;
    current_screen_name_ = name;
    target->OnActivate();
    LOG_INFO(atom::log::core::ScreenManager, "Switched screen to : " + name);
    return ScreenOpResult::Success;

}

auto ScreenManager::SwitchScreen(Screen* screen) -> ScreenOpResult {
    if (screen == nullptr) {
        LOG_WARNING(atom::log::core::ScreenManager, "SwitchScreen called with a null screen");
        return ScreenOpResult::NullScreen;
    }
    const auto name = GetScreenName(screen);
    if (name.empty()) {
        LOG_WARNING(atom::log::core::ScreenManager, "SwitchScreen called with an unregistered screen");
        return ScreenOpResult::Unregistered;
    }
    return SwitchScreen(name);
}

auto ScreenManager::PushScreen(const std::string& name) -> ScreenOpResult {
    const auto it = screens_.find(name);
    if (it == screens_.end() || !it->second) {
        LOG_WARNING(atom::log::core::ScreenManager, "PushScreen called with an unregistered screen: " + name);
        return ScreenOpResult::NotFound;
    }
    // Copy the handle out before any callback: OnDeactivate() may re-enter the
    // manager and invalidate the map iterator.
    const auto target = it->second;
    if (current_screen_ == target) {
        LOG_DEBUG(atom::log::core::ScreenManager, "PushScreen ignored: '" + name + "' is already active");
        return ScreenOpResult::AlreadyVisible;
    }

    // Container updates first, lifecycle callbacks last (see LoadScreen).
    auto previous = current_screen_;
    const auto previous_name = current_screen_name_;
    if (previous)
        screen_stack_.emplace_back(previous_name, previous);

    current_screen_ = target;
    current_screen_name_ = name;

    if (previous)
        previous->OnDeactivate();
    target->OnActivate();
    LOG_INFO(atom::log::core::ScreenManager, "Pushed & Rendering screen: " + name);
    return ScreenOpResult::Success;
}

auto ScreenManager::PushScreen(Screen* screen) -> ScreenOpResult {
    if (screen == nullptr) {
        LOG_WARNING(atom::log::core::ScreenManager, "PushScreen called with a null screen");
        return ScreenOpResult::NullScreen;
    }
    const auto name = GetScreenName(screen);
    if (name.empty()) {
        LOG_WARNING(atom::log::core::ScreenManager, "PushScreen called with an unregistered screen");
        return ScreenOpResult::Unregistered;
    }
    return PushScreen(name);
}

auto ScreenManager::PopScreen() -> ScreenOpResult {
    if (screen_stack_.empty()) {
        LOG_DEBUG(atom::log::core::ScreenManager, "PopScreen ignored: the screen stack is empty");
        return ScreenOpResult::EmptyStack;
    }

    // Move the target out of the stack before deactivating, so a re-entrant
    // callback cannot leave `current_screen_` dangling (the handle stays alive
    // in `restored`).
    auto restored = std::move(screen_stack_.back());
    screen_stack_.pop_back();

    auto previous = current_screen_;
    if (previous)
        previous->OnDeactivate();

    current_screen_name_ = restored.first;
    current_screen_ = std::move(restored.second);
    current_screen_->OnActivate();
    LOG_INFO(atom::log::core::ScreenManager, "Popped to screen: " + current_screen_name_);
    return ScreenOpResult::Success;
}

auto ScreenManager::Render(atom::render::IRenderDevice& device) const -> void {
    for (const auto& [name, screen] : screen_stack_) {
        screen->Render(device);
    }
    if (current_screen_) {
        current_screen_->Render(device);
    }
}

auto ScreenManager::HandleEvent(const atom::window::IEvent& event) const -> void {
    if (current_screen_) {
        if (current_screen_->HandleEvent(event)) {
            return;
        }
    }
    for (const auto& screen : screen_stack_ | std::views::values) {
        if (screen->HandleEvent(event)) {
            return;
        }
    }
}

auto ScreenManager::Update(const float deltaTime) const -> void {
    if (current_screen_) {
        current_screen_->Update(deltaTime);
    }
}

auto ScreenManager::FixedUpdate(const float deltaTime) const -> void {
    if (current_screen_) {
        current_screen_->FixedUpdate(deltaTime);
    }
}

auto ScreenManager::GetCurrentScreenName() const -> const std::string& {
    return current_screen_name_;
}

auto ScreenManager::GetCurrentScreen() const -> Screen* {
    return current_screen_.get();
}

auto ScreenManager::GetScreenStack() const -> std::vector<ScreenStackEntry> {
    std::vector<ScreenStackEntry> entries{};
    entries.reserve(screen_stack_.size());
    for (const auto& [name, screen] : screen_stack_)
        entries.push_back(ScreenStackEntry{name, screen.get()});
    return entries;
}

auto ScreenManager::IsScreenVisible(const Screen* screen) const -> bool {
    if (screen == nullptr)
        return false;
    if (current_screen_.get() == screen)
        return true;
    for (const auto& entry : screen_stack_) {
        if (entry.second.get() == screen)
            return true;
    }
    return false;
}
} // namespace atom
