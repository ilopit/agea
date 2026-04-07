#include "engine/private/ui/action_progress_window.h"

#include <global_state/global_state.h>

#include <imgui.h>

namespace kryga
{
namespace ui
{

void
action_progress_window::handle()
{
    auto& actions = glob::glob_state().getr_ui().m_actions;

    // Auto-show when busy
    if (actions.is_busy() && !m_show)
    {
        m_show = true;
    }

    if (!handle_begin())
    {
        return;
    }

    // Current action
    if (actions.is_busy())
    {
        auto* progress = actions.current_progress();
        float p = progress->progress.load();
        std::string status = progress->get_status();

        ImGui::Text("Running: %s", status.c_str());
        ImGui::ProgressBar(p, ImVec2(-1, 0));

        size_t queued = actions.queued_count();
        if (queued > 0)
        {
            ImGui::Text("%zu action(s) queued", queued);
        }

        ImGui::Separator();
    }
    else
    {
        ImGui::TextDisabled("Idle");
        ImGui::Separator();
    }

    // Completed actions
    auto& finished = actions.finished();
    if (!finished.empty())
    {
        ImGui::Text("Recent:");
        for (auto it = finished.rbegin(); it != finished.rend(); ++it)
        {
            if (it->success)
            {
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "[OK] %s (%.0fms)",
                                   it->name.c_str(), it->duration_ms);
            }
            else
            {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[FAIL] %s: %s",
                                   it->name.c_str(), it->error.c_str());
            }
        }

        if (ImGui::Button("Clear"))
        {
            actions.clear_finished();
        }
    }

    handle_end();
}

}  // namespace ui
}  // namespace kryga
