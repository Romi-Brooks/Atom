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

auto LevelName(const LogLevel level) -> const char* {
    switch (level) {
    case LogLevel::ATOM_DEBUG:
        return "DEBUG";
    case LogLevel::ATOM_INFO:
        return "INFO";
    case LogLevel::ATOM_WARNING:
        return "WARNING";
    case LogLevel::ATOM_ERROR:
        return "ERROR";
    }
    return "UNKNOWN";
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
    log_connection_ = Log::Subscribe([weak_state, max_entries = max_entries_](const LogRecord& record) {
        if (const auto state = weak_state.lock()) {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->records.push_back(record);
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

    std::vector<LogRecord> records;
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        records.assign(state_->records.begin(), state_->records.end());
    }

    auto channels = Log::GetRegisteredChannels();
    std::unordered_set<std::string> known_channels;
    for (const auto& channel : channels)
        known_channels.insert(ChannelKey(channel));
    for (const auto& record : records) {
        LogChannelInfo channel{record.channel_prefix, record.channel_name};
        if (known_channels.insert(ChannelKey(channel)).second)
            channels.push_back(std::move(channel));
    }

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

    ImGui::Text("Buffered: %zu", records.size());
    ImGui::Separator();

    ImGui::BeginChild("LogEntries", ImVec2{0.0f, 0.0f}, true, ImGuiWindowFlags_HorizontalScrollbar);
    const bool was_at_bottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;
    bool displayed_record = false;
    for (const auto& record : records) {
        if (record.level < minimum_level_)
            continue;

        const std::string channel = record.channel_prefix + record.channel_name;
        if (!selected_channels_.empty() && !selected_channels_.contains(channel))
            continue;
        if (text_filter_[0] != '\0' && record.message.find(text_filter_) == std::string::npos)
            continue;

        const std::string line = "[" + record.timestamp + "] [" + LevelName(record.level) + "] " + channel +
                                 " -> " + record.message;
        ImGui::PushStyleColor(ImGuiCol_Text, LevelColor(record.level));
        ImGui::TextUnformatted(line.c_str());
        ImGui::PopStyleColor();
        displayed_record = true;
    }
    if (auto_scroll_ && was_at_bottom && displayed_record)
        ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
    ImGui::End();

    if (!window_open_)
        DebugPanel::SetEnabled(false);
}

} // namespace atom::debugger
