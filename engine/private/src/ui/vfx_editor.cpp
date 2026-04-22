#include "engine/private/ui/vfx_editor.h"

#include <imgui.h>

#include <glm_unofficial/glm.h>

#include <algorithm>
#include <cstdio>

namespace kryga
{
namespace ui
{

namespace
{

ImU32
packed_color(const glm::vec4& c)
{
    auto clamp01 = [](float v) { return std::clamp(v, 0.0f, 1.0f); };
    return IM_COL32(static_cast<int>(clamp01(c.r) * 255.0f),
                    static_cast<int>(clamp01(c.g) * 255.0f),
                    static_cast<int>(clamp01(c.b) * 255.0f),
                    static_cast<int>(clamp01(c.a) * 255.0f));
}

}  // namespace

vfx_editor::vfx_editor()
    : window(window_title())
{
    m_system.create("default_particles", vfx::make_preset(vfx::preset::particles));
    m_selected = 0;
}

void
vfx_editor::handle()
{
    if (!handle_begin())
    {
        return;
    }

    float dt = ImGui::GetIO().DeltaTime;
    if (!m_paused)
    {
        m_system.tick(dt);
    }

    draw_toolbar();
    ImGui::Separator();

    ImGui::Columns(2, "vfx_cols", true);
    ImGui::SetColumnWidth(-1, 260.0f);

    draw_emitter_list();

    ImGui::NextColumn();

    auto& entries = m_system.entries();
    if (m_selected >= 0 && m_selected < static_cast<int>(entries.size()))
    {
        draw_params(*entries[m_selected].e);
    }
    else
    {
        ImGui::TextDisabled("No emitter selected");
    }

    ImGui::Columns(1);
    ImGui::Separator();
    draw_preview();

    handle_end();
}

void
vfx_editor::draw_toolbar()
{
    const char* preset_labels[vfx::preset_count] = {
        vfx::preset_name(vfx::preset::particles),
        vfx::preset_name(vfx::preset::smoke),
        vfx::preset_name(vfx::preset::dust),
    };
    ImGui::SetNextItemWidth(120.0f);
    ImGui::Combo("##preset", &m_new_preset, preset_labels, vfx::preset_count);
    ImGui::SameLine();
    if (ImGui::Button("Add"))
    {
        auto p = static_cast<vfx::preset>(m_new_preset);
        char name[32];
        std::snprintf(name, sizeof(name), "%s_%zu", vfx::preset_name(p), m_system.entries().size());
        m_system.create(name, vfx::make_preset(p));
        m_selected = static_cast<int>(m_system.entries().size()) - 1;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Pause", &m_paused);
    ImGui::SameLine();
    ImGui::Checkbox("Side view", &m_side_view);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::SliderFloat("zoom", &m_zoom, 20.0f, 600.0f, "%.0f");
}

void
vfx_editor::draw_emitter_list()
{
    auto& entries = m_system.entries();

    ImGui::Text("Emitters (%zu)", entries.size());
    ImGui::BeginChild("emitter_list", ImVec2(0, 220.0f), true);
    for (int i = 0; i < static_cast<int>(entries.size()); ++i)
    {
        ImGui::PushID(i);
        bool is_sel = m_selected == i;
        if (ImGui::Selectable(entries[i].name.c_str(), is_sel))
        {
            m_selected = i;
        }
        ImGui::PopID();
    }
    ImGui::EndChild();

    if (m_selected >= 0 && m_selected < static_cast<int>(entries.size()))
    {
        if (ImGui::Button("Remove"))
        {
            m_system.remove(entries[m_selected].e.get());
            m_selected = -1;
            return;
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Particles"))
        {
            entries[m_selected].e->clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Burst 32"))
        {
            entries[m_selected].e->spawn_burst(32);
        }
    }
}

void
vfx_editor::draw_params(vfx::emitter& e)
{
    auto& pr = e.params();

    ImGui::Checkbox("Enabled", &e.enabled);
    ImGui::DragFloat3("Origin", &e.origin.x, 0.05f);

    ImGui::Separator();
    ImGui::Text("Spawn");

    const char* shape_names[] = {"point", "sphere", "box"};
    int shape = static_cast<int>(pr.shape);
    if (ImGui::Combo("Shape", &shape, shape_names, IM_ARRAYSIZE(shape_names)))
    {
        pr.shape = static_cast<vfx::spawn_shape>(shape);
    }
    ImGui::DragFloat3("Shape extents", &pr.shape_extents.x, 0.05f, 0.0f, 100.0f);

    ImGui::DragFloat("Spawn rate", &pr.spawn_rate, 1.0f, 0.0f, 10000.0f, "%.1f / s");

    int max_p = static_cast<int>(pr.max_particles);
    if (ImGui::DragInt("Max particles", &max_p, 4.0f, 0, 65536))
    {
        pr.max_particles = static_cast<std::uint32_t>(std::max(0, max_p));
    }

    ImGui::Separator();
    ImGui::Text("Motion");

    ImGui::DragFloat3("Velocity dir", &pr.initial_velocity_dir.x, 0.05f, -1.0f, 1.0f);
    ImGui::SliderAngle("Cone angle", &pr.velocity_cone_angle_rad, 0.0f, 180.0f);
    ImGui::DragFloat("Speed", &pr.velocity_speed, 0.05f, 0.0f, 100.0f);
    ImGui::DragFloat("Speed jitter", &pr.velocity_speed_jitter, 0.05f, 0.0f, 100.0f);

    ImGui::DragFloat3("Acceleration", &pr.acceleration.x, 0.05f, -100.0f, 100.0f);
    ImGui::DragFloat("Drag", &pr.drag, 0.01f, 0.0f, 10.0f);

    ImGui::Separator();
    ImGui::Text("Life & look");

    ImGui::DragFloat("Lifetime", &pr.lifetime, 0.05f, 0.01f, 60.0f, "%.2f s");
    ImGui::DragFloat("Lifetime jitter", &pr.lifetime_jitter, 0.05f, 0.0f, 60.0f, "%.2f s");

    ImGui::ColorEdit4("Color start", &pr.color_start.x);
    ImGui::ColorEdit4("Color end", &pr.color_end.x);
    ImGui::DragFloat("Size start", &pr.size_start, 0.01f, 0.0f, 100.0f);
    ImGui::DragFloat("Size end", &pr.size_end, 0.01f, 0.0f, 100.0f);

    ImGui::Separator();
    ImGui::Text("Runtime: %zu particles", e.get_particles().size());
}

void
vfx_editor::draw_preview()
{
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float canvas_h = std::max(180.0f, avail.y);
    ImVec2 canvas_size{avail.x, canvas_h};
    ImVec2 cursor = ImGui::GetCursorScreenPos();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(cursor,
                      ImVec2(cursor.x + canvas_size.x, cursor.y + canvas_size.y),
                      IM_COL32(10, 10, 14, 255));
    dl->AddRect(cursor,
                ImVec2(cursor.x + canvas_size.x, cursor.y + canvas_size.y),
                IM_COL32(60, 60, 70, 255));

    ImVec2 center{cursor.x + canvas_size.x * 0.5f, cursor.y + canvas_size.y * 0.5f};

    dl->AddLine(ImVec2(center.x, cursor.y),
                ImVec2(center.x, cursor.y + canvas_size.y),
                IM_COL32(40, 40, 50, 255));
    dl->AddLine(ImVec2(cursor.x, center.y),
                ImVec2(cursor.x + canvas_size.x, center.y),
                IM_COL32(40, 40, 50, 255));

    for (auto& entry : m_system.entries())
    {
        for (const auto& p : entry.e->get_particles())
        {
            float u = m_side_view ? p.position.x : p.position.x;
            float v = m_side_view ? -p.position.y : p.position.z;

            float sx = center.x + u * m_zoom;
            float sy = center.y + v * m_zoom;

            if (sx < cursor.x || sx > cursor.x + canvas_size.x || sy < cursor.y ||
                sy > cursor.y + canvas_size.y)
            {
                continue;
            }

            float r = std::max(1.5f, p.size * m_zoom * 0.5f);
            dl->AddCircleFilled(ImVec2(sx, sy), r, packed_color(p.color));
        }
    }

    ImGui::Dummy(canvas_size);
    ImGui::TextDisabled("%s view  (drag zoom)", m_side_view ? "side (XY)" : "top (XZ)");
}

}  // namespace ui
}  // namespace kryga
