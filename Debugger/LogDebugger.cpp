// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include "LogDebugger.hpp"

#include <string>
#include <utility>
#include <vector>

#include <Debugger/PanelLayout.hpp>
#include <Log/LogSystem.hpp>
#include <Window/RenderWindow.hpp>

#include <imgui.h>

namespace atom::debugger {
namespace {

LogDebugger* g_log_debugger_instance = nullptr;

// A LogRecord plus everything the viewer renders, precomputed exactly once when
// the record arrives. This keeps the hot per-frame path to pointer copies +
// ImGuiListClipper instead of rebuilding a string for every buffered line on
// every frame (the old behavior, which scaled badly as the buffer filled).
struct DisplayRecord {
        LogRecord record;
        std::string channel;   // prefix + name
        std::string line;      // "[timestamp] [LEVEL] channel -> message"
        std::string location;  // "file:line function" (empty when unspecified)
};

auto BuildDisplayRecord(const LogRecord& record) -> std::shared_ptr<const DisplayRecord> {
    auto display = std::make_shared<DisplayRecord>();
    display->record = record;
    display->channel = record.channel_prefix + record.channel_name;
    display->line = "[" + record.timestamp + "] [" + atom::ToString(record.level) + "] " + display->channel +
                    " -> " + record.message;
    if (!record.file.empty()) {
        display->location = record.file;
        if (record.line > 0)
            display->location += ":" + std::to_string(record.line);
        if (!record.function.empty())
            display->location += " " + record.function;
    }
    return display;
}

auto LevelColor(const LogLevel level) -> ImVec4 {
    switch (level) {
    case LogLevel::ATOM_DEBUG:
        return ImVec4{0.65f, 0.65f, 0.65f, 1.0f};
    case LogLevel::ATOM_INFO:
        return ImVec4{0.75f, 0.85f, 1.0f, 1.0f};
    case LogLevel::ATOM_WARNING:
        return ImVec4{1.0f, 0.85f, 0.35f, 1.0f};
    case LogLevel::ATOM_ERROR:
        return ImVec4{1.0f, 0.35f, 0.35f, 1.0f};
    }
    return ImVec4{1.0f, 1.0f, 1.0f, 1.0f};
}

auto ChannelKey(const LogChannelInfo& channel) -> std::string {
    return channel.prefix + channel.name;
}

} // namespace

// Records are shared_ptr so the render thread can snapshot a filtered view
// without deep-copying every LogRecord (each holds several strings). The
// pointed-to DisplayRecord stays alive via the shared_ptr even if the ring
// evicts it mid-frame.
struct LogDebugger::State {
        std::mutex mutex;
        std::deque<std::shared_ptr<const DisplayRecord>> records;
};

LogDebugger::~LogDebugger() {
    // Detach while this derived object is still alive so OnDetach dispatches here.
    Detach();
    if (g_log_debugger_instance == this)
        g_log_debugger_instance = nullptr;
}

auto LogDebugger::Get() -> LogDebugger* {
#if ATOM_ENABLE_DEBUGGER
    return g_log_debugger_instance;
#else
    return nullptr;
#endif
}

auto LogDebugger::SetEnabled(const bool enabled) -> void {
    DebugPanel::SetEnabled(enabled);
    window_open_ = enabled;
    if (enabled)
        slot_applied_ = false;
}

auto LogDebugger::OnAttach(atom::RenderWindow&) -> bool {
    if (g_log_debugger_instance != nullptr && g_log_debugger_instance != this) {
        LOG_WARNING(atom::log::debugger::ImGui,
                    "LogDebugger already attached; rejecting a second instance");
        return false;
    }
    g_log_debugger_instance = this;
    state_ = std::make_shared<State>();
    const auto weak_state = std::weak_ptr<State>{state_};
    // Log::Subscribe replays the buffered recent records (ARCH-110), so the
    // resource-preparation and asset-loading lines emitted before this panel
    // existed still show up in the viewer.
    log_connection_ = Log::Subscribe([weak_state, max_entries = max_entries_](const LogRecord& record) {
        if (const auto state = weak_state.lock()) {
            auto display = BuildDisplayRecord(record);
            std::lock_guard<std::mutex> lock(state->mutex);
            state->records.push_back(std::move(display));
            while (state->records.size() > max_entries)
                state->records.pop_front();
        }
    });
    window_open_ = true;
    slot_applied_ = false;
    return true;
}

auto LogDebugger::OnDetach() -> void {
    if (g_log_debugger_instance == this)
        g_log_debugger_instance = nullptr;
    log_connection_.Reset();
    state_.reset();
    slot_applied_ = false;
}

auto LogDebugger::OnDrawOverlay() -> void {
    if (!state_ || !window_open_)
        return;

    if (!slot_applied_) {
        ApplyLogPanelSlot();
        slot_applied_ = true;
    }

    if (!ImGui::Begin("Log Debugger", &window_open_)) {
        ImGui::End();
        if (!window_open_)
            DebugPanel::SetEnabled(false);
        return;
    }

    // Snapshot a filtered view without deep-copying records: shared_ptr copies
    // keep each entry alive for the remainder of this frame.
    std::vector<std::shared_ptr<const DisplayRecord>> filtered;
    std::size_t buffered = 0;
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        buffered = state_->records.size();
        filtered.reserve(buffered);
        for (const auto& display : state_->records) {
            if (display->record.level < minimum_level_)
                continue;
            if (!selected_channels_.empty() && !selected_channels_.contains(display->channel))
                continue;
            if (text_filter_[0] != '\0' &&
                display->record.message.find(text_filter_) == std::string::npos)
                continue;
            filtered.push_back(display);
        }
    }

    // Channel list refreshes at most once per second: it scans the ring, so
    // doing it every frame would redo string keying for the whole buffer.
    const double now = ImGui::GetTime();
    if (channels_refresh_time_ < 0.0 || now - channels_refresh_time_ >= 1.0) {
        channels_refresh_time_ = now;
        auto channels = Log::GetRegisteredChannels();
        std::unordered_set<std::string> known;
        for (const auto& channel : channels)
            known.insert(ChannelKey(channel));
        {
            std::lock_guard<std::mutex> lock(state_->mutex);
            for (const auto& display : state_->records) {
                LogChannelInfo channel{display->record.channel_prefix, display->record.channel_name};
                if (known.insert(ChannelKey(channel)).second)
                    channels.push_back(std::move(channel));
            }
        }
        cached_channels_ = std::move(channels);
    }
    auto& channels = cached_channels_;

    const char* level_names[] = {"DEBUG", "INFO", "WARNING", "ERROR"};
    int level_index = static_cast<int>(minimum_level_);
    ImGui::SetNextItemWidth(110.0f);
    if (ImGui::Combo("Minimum level", &level_index, level_names, 4))
        minimum_level_ = static_cast<LogLevel>(level_index);
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &auto_scroll_);
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        std::lock_guard<std::mutex> lock(state_->mutex);
        state_->records.clear();
    }

    std::string channel_summary = selected_channels_.empty() ? "All channels" :
        std::to_string(selected_channels_.size()) + " selected";
    if (ImGui::BeginCombo("Channels", channel_summary.c_str())) {
        const bool all_selected = selected_channels_.empty();
        if (ImGui::Selectable("All channels", all_selected, ImGuiSelectableFlags_DontClosePopups))
            selected_channels_.clear();
        ImGui::Separator();
        for (const auto& channel : channels) {
            const auto key = ChannelKey(channel);
            bool selected = selected_channels_.contains(key);
            if (ImGui::Selectable(key.c_str(), selected, ImGuiSelectableFlags_DontClosePopups)) {
                if (selected)
                    selected_channels_.erase(key);
                else
                    selected_channels_.insert(key);
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::InputText("Message", text_filter_, sizeof(text_filter_));

    ImGui::Text("Buffered: %zu (showing %zu)", state_->records.size(), filtered.size());
    ImGui::Separator();

    ImGui::BeginChild("LogEntries", ImVec2{0.0f, 0.0f}, true, ImGuiWindowFlags_HorizontalScrollbar);
    const bool was_at_bottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;

    // Only the visible rows are submitted. Buffering 1024 records is cheap once
    // rendering no longer iterates the whole ring every frame.
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(filtered.size()));
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            const auto& display = filtered[static_cast<std::size_t>(i)];
            ImGui::PushStyleColor(ImGuiCol_Text, LevelColor(display->record.level));
            ImGui::TextUnformatted(display->line.c_str());
            ImGui::PopStyleColor();
            if (!display->location.empty()) {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.55f, 0.55f, 0.55f, 1.0f});
                ImGui::TextUnformatted(display->location.c_str());
                ImGui::PopStyleColor();
            }
        }
    }
    if (auto_scroll_ && was_at_bottom)
        ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
    ImGui::End();

    if (!window_open_)
        DebugPanel::SetEnabled(false);
}

} // namespace atom::debugger
