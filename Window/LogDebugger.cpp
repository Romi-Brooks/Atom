#include "LogDebugger.hpp"

#include <string>
#include <utility>
#include <vector>

#include <Log/LogSystem.hpp>
#include <Window/OverlayManager.hpp>
#include <Window/RenderWindow.hpp>

#include <imgui.h>

namespace atom {
namespace {

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
    Detach();
}

auto LogDebugger::Attach(RenderWindow& window) -> void {
    if (attached_)
        return;

    target_window_ = &window;
    if (!window.GetIWindow() || !window.GetRenderDevice()) {
        target_window_ = nullptr;
        return;
    }

    auto& overlay_manager = window.GetOverlayManager();
    overlay_connection_ = std::make_unique<debugger::OverlayConnection>(
        overlay_manager.AddPanel([this] {
            if (enabled_)
                OnDrawOverlay();
        }));
    if (!overlay_connection_->IsConnected()) {
        overlay_connection_.reset();
        target_window_ = nullptr;
        return;
    }

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
    enabled_ = true;
    attached_ = true;
}

auto LogDebugger::Detach() -> void {
    log_connection_.Reset();
    state_.reset();
    overlay_connection_.reset();
    target_window_ = nullptr;
    attached_ = false;
}

auto LogDebugger::SetEnabled(const bool enabled) -> void {
    window_open_ = enabled;
    enabled_ = enabled;
}

auto LogDebugger::OnDrawOverlay() -> void {
    if (!state_ || !window_open_)
        return;

    if (!ImGui::Begin("Log Debugger", &window_open_)) {
        ImGui::End();
        if (!window_open_)
            enabled_ = false;
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
    // Ad-hoc string channels are still captured. Add them to the selector as
    // observed channels so game code does not need an enum to be visible here.
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
        enabled_ = false;
}

} // namespace atom
