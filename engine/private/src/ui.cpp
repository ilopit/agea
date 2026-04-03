#include "engine/ui.h"

#include <global_state/global_state.h>

#include "engine/script_editor.h"
#include "engine/console.h"
#include "engine/property_drawers.h"
#include "engine/engine_counters.h"

#include "engine/private/ui/package_editor.h"
#include "engine/private/ui/object_editor.h"
#include "engine/private/ui/gizmo_editor.h"
#include "engine/editor.h"

#include <core/level.h>
#include <core/caches/caches_map.h>
#include <core/reflection/property.h>
#include <core/reflection/lua_api.h>

#include <packages/root/model/game_object.h>
#include <packages/root/model/assets/material.h>

#include <core/package.h>
#include <core/package_manager.h>

#include <native/native_window.h>

#include <vulkan_render/types/vulkan_texture_data.h>
#include <vulkan_render/vulkan_render_device.h>
#include <vulkan_render/kryga_render.h>
#include <gpu_types/gpu_shadow_types.h>

#include <SDL.h>
#include <SDL_vulkan.h>

#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_vulkan.h>

#include <ImGuizmo.h>

#include <algorithm>
#include <array>

namespace kryga
{

void
state_mutator__ui::set(gs::state& s)
{
    auto p = s.create_box<ui::ui>("ui");
    s.m_ui = p;
}

namespace ui
{

static gizmo_editor s_gizmo_editor;

ui::ui()
{
    property_drawers::init();

    m_windows[level_editor_window::window_title()] = std::make_unique<level_editor_window>();
    m_windows[level_editor_window::window_title()]->m_show = true;

    m_windows[materials_selector::window_title()] = std::make_unique<materials_selector>();
    m_windows[object_editor::window_title()] = std::make_unique<object_editor>();
    m_windows[components_editor::window_title()] = std::make_unique<components_editor>();

    m_windows[editor_console::window_title()] = std::make_unique<editor_console>();
    m_windows[editor_console::window_title()]->m_show = true;

    m_windows[package_editor::window_title()] = std::make_unique<package_editor>();
    m_windows[package_editor::window_title()]->m_show = true;

    m_windows[performance_counters_window::window_title()] =
        std::make_unique<performance_counters_window>();
    m_windows[performance_counters_window::window_title()]->m_show = true;

    m_windows[render_config_window::window_title()] = std::make_unique<render_config_window>();
    m_windows[render_config_window::window_title()]->m_show = true;
}

ui::~ui()
{
    if (ImGui::GetCurrentContext())
    {
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
    }
}

void
ui::init()
{
    // Init ImGui
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_ImplSDL2_InitForVulkan(glob::glob_state().getr_native_window().handle());

    // Color scheme
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.5f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.0f, 0.0f, 0.0f, 0.1f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.8f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.1f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.2f);
    style.Colors[ImGuiCol_Button] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
}

void
ui::new_frame(float dt)
{
    ImGuiIO& io = ImGui::GetIO();
    auto s = glob::glob_state().getr_native_window().get_size();

    io.DisplaySize = ImVec2((float)s.w, (float)s.h);
    io.DeltaTime = dt;

    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // Fullscreen dockspace — all windows can dock into this, central node is transparent for 3D
    // viewport
    ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

    bool playing = glob::glob_state().getr_game_editor().get_mode() == engine::editor_mode::playing;

    if (playing)
    {
        // Only show performance counters during play mode
        auto it = m_windows.find(performance_counters_window::window_title());
        if (it != m_windows.end())
        {
            it->second->handle();
        }

        // PLAYING indicator overlay
        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x * 0.5f, 8.f), ImGuiCond_Always, ImVec2(0.5f, 0.f));
        ImGui::SetNextWindowSize(ImVec2(0, 0));
        ImGui::Begin("##play_indicator",
                     nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                         ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing);
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "PLAYING (F5/Esc to stop)");
        ImGui::End();
    }
    else
    {
        for (auto& w : m_windows)
        {
            w.second->handle();
        }
    }

    ImGuizmo::BeginFrame();

    s_gizmo_editor.draw();

    ImGui::Render();
}

void
window::handle()
{
    ImGui::Begin(m_str.data(), &m_show, ImGuiWindowFlags_MenuBar);

    ImGui::End();
}

bool
window::handle_begin(int flags)
{
    if (!m_show)
    {
        return false;
    }

    ImGui::Begin(m_str.data(), &m_show, flags);

    return true;
}

void
window::handle_end()
{
    ImGui::End();
}

void
level_editor_window::handle()
{
    if (!handle_begin(ImGuiWindowFlags_MenuBar))
    {
        return;
    }

    auto level = ::kryga::glob::glob_state().get_current_level();

    if (ImGui::TreeNode("Level Objects"))
    {
        for (auto& o : level->get_game_objects().get_items())
        {
            if (auto game_obj = o.second->as<root::game_object>())
            {
                draw_object(game_obj);
            }
        }
        ImGui::TreePop();
    }
    handle_end();
}

void
level_editor_window::draw_object(root::game_object* obj)
{
    bool opened = ImGui::TreeNode(obj->get_id().cstr());
    if (ImGui::IsItemClicked())
    {
        get_window<object_editor>()->show(obj);
    }
    if (opened)
    {
        ImGui::TreePop();
    }
}

void
materials_selector::handle()
{
    if (!handle_begin())
    {
        return;
    }

    ImGui::Text("Select material:");
    ImGui::InputText("##material", m_filtering_text.data(), m_filtering_text.size(), 0);
    ImGui::Separator();

    glob::glob_state().get_class_materials_cache()->call_on_items(
        [this](root::material* m)
        {
            {
                auto mat_id = m->get_id().str();
                if (mat_id.find(m_filtering_text.data()) == std::string::npos)
                {
                    return true;
                }

                auto open = ImGui::TreeNodeEx(mat_id.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
                if (open)
                {
                    ImGui::Separator();
                    ImGui::Columns(2);
                    ImGui::SetColumnWidth(-1, 120);
                    ImGui::Text("Preview");
                    if (ImGui::Button("Use"))
                    {
                        m_selection_cb(mat_id);
                    }
                    ImGui::NextColumn();
                    // ImGui::Image(m->get_material_data()->texture_set, ImVec2{160, 160});

                    ImGui::Columns(1);

                    ImGui::Separator();
                    ImGui::TreePop();
                }
            }
            return true;
        });

    handle_end();
}

void
components_editor::handle()
{
    if (!handle_begin(ImGuiWindowFlags_NoFocusOnAppearing))
    {
        return;
    }

    ImGui::Columns(2);
    ImGui::Separator();
    auto& list = m_obj->get_reflection()->m_editor_properties;

    for (auto& categories : list)
    {
        ImGui::Columns(1);
        ImGui::Separator();
        ImGui::Text("%s", categories.first.c_str());
        ImGui::Separator();
        ImGui::Columns(2);

        for (auto& p : categories.second)
        {
            ImGui::Text("%s", p->name.data());
            ImGui::NextColumn();
            if (p->name == "material_id")
            {
                if (ImGui::Button("e"))
                {
                    auto handler = [&p, this](const std::string& selected)
                    {
                        auto str = (std::string*)((char*)m_obj + p->offset);

                        (*str) = selected;

                        // m_obj->mark_dirty();
                    };

                    get_window<materials_selector>()->show(handler);
                }
                ImGui::SameLine();
            }
            property_drawers::draw_ro(m_obj, *p);
            ImGui::NextColumn();
        }
    }

    handle_end();
}

void
performance_counters_window::handle()
{
    if (!lock)
    {
        frame_avg = glob::glob_state().getr_engine_counters().frame.avg / 1000;
        fps = 1000000.0 / glob::glob_state().getr_engine_counters().frame.avg;
        input_avg = glob::glob_state().getr_engine_counters().input.avg / 1000;
        tick_avg = glob::glob_state().getr_engine_counters().tick.avg / 1000;
        ui_tick_avg = glob::glob_state().getr_engine_counters().ui_tick.avg / 1000;
        consume_updates_avg = glob::glob_state().getr_engine_counters().consume_updates.avg / 1000;
        draw_avg = glob::glob_state().getr_engine_counters().draw.avg / 1000;
        culled_draws_avg = glob::glob_state().getr_engine_counters().culled_draws.avg;
        all_draws_avg = glob::glob_state().getr_engine_counters().all_draws.avg;
        objects_avg = glob::glob_state().getr_engine_counters().objects.avg;
        lock = 24;
    }

    ImGui::Separator();
    ImGui::Text("Frame   : %3.3lf", frame_avg);
    ImGui::Text("FPS     : %3.3lf", fps);
    ImGui::Text("Input   : %3.3lf", input_avg);
    ImGui::Separator();
    ImGui::Text("Tick    : %3.3lf", tick_avg);
    ImGui::Text("UI tick : %3.3lf", ui_tick_avg);
    ImGui::Text("Update  : %3.3lf", consume_updates_avg);
    ImGui::Separator();
    ImGui::Text("Draw    : %3.3lf", draw_avg);
    ImGui::Separator();
    ImGui::Text("Objects : %3.3lf", objects_avg);
    ImGui::Text("Draws   : %3.3lf", all_draws_avg);
    ImGui::Text("Cull %  : %3.3lf", culled_draws_avg / all_draws_avg * 100);
    ImGui::Separator();

    // Render mode is set at startup (no runtime switching)
    bool is_instanced = glob::glob_state().getr_vulkan_render().is_instanced_mode();
    ImGui::Text("Mode: %s", is_instanced ? "INSTANCED" : "PER_OBJECT");

    --lock;
}

void
render_config_window::handle()
{
    if (!handle_begin())
    {
        return;
    }

    auto& vr = glob::glob_state().getr_vulkan_render();
    auto& shadow = vr.get_shadow_config();

    // =========================================================================
    // Shadows
    // =========================================================================
    if (ImGui::CollapsingHeader("Shadows", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Defaults
        constexpr uint32_t DEF_PCF_MODE = KGPU_PCF_POISSON16;
        constexpr float DEF_SHADOW_BIAS = 0.005f;
        constexpr float DEF_NORMAL_BIAS = 0.03f;
        constexpr int DEF_CASCADE_COUNT = KGPU_CSM_CASCADE_COUNT;
        constexpr float DEF_SHADOW_DISTANCE = 200.0f;

        // Helper: small reset button on the same line, right-aligned
        auto reset_button = [](const char* id, const char* tip) -> bool
        {
            ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 18);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            bool clicked = ImGui::SmallButton(id);
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", tip);
            }
            return clicked;
        };

        // PCF Mode
        const char* pcf_names[] = {
            "3x3 (9 taps)", "5x5 (25 taps)", "7x7 (49 taps)", "Poisson 16", "Poisson 32"};
        int pcf_mode = static_cast<int>(shadow.directional.pcf_mode);
        if (ImGui::Combo("PCF Mode", &pcf_mode, pcf_names, IM_ARRAYSIZE(pcf_names)))
        {
            shadow.directional.pcf_mode = static_cast<uint32_t>(pcf_mode);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Shadow edge softening filter.\n\n"
                "Grid NxN: regular sample pattern, higher = softer but more costly.\n"
                "Poisson: irregular disk pattern, less banding than grid at similar cost.");
        }
        if (reset_button("D##pcf", "Reset to default: Poisson 16"))
        {
            shadow.directional.pcf_mode = DEF_PCF_MODE;
        }

        // Bias
        float bias = shadow.directional.shadow_bias;
        if (ImGui::DragFloat("Shadow Bias", &bias, 0.0001f, 0.0f, 0.1f, "%.4f"))
        {
            shadow.directional.shadow_bias = bias;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Constant depth offset to prevent shadow acne.\n\n"
                "Too low: moire/stripe patterns on lit surfaces.\n"
                "Too high: shadows detach from caster base (peter-panning).\n"
                "Ctrl+click to type exact value.");
        }
        if (reset_button("D##bias", "Reset to default: 0.0050"))
        {
            shadow.directional.shadow_bias = DEF_SHADOW_BIAS;
        }

        // Normal Bias
        float normal_bias = shadow.directional.normal_bias;
        if (ImGui::DragFloat("Normal Bias", &normal_bias, 0.001f, 0.0f, 0.5f, "%.3f"))
        {
            shadow.directional.normal_bias = normal_bias;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Offsets shadow lookup along the surface normal.\n\n"
                "Helps eliminate acne on surfaces at grazing angles to the light.\n"
                "Too high: thin shadow gaps appear at object edges.\n"
                "Ctrl+click to type exact value.");
        }
        if (reset_button("D##nbias", "Reset to default: 0.030"))
        {
            shadow.directional.normal_bias = DEF_NORMAL_BIAS;
        }

        ImGui::Spacing();

        // Cascade count
        int cascade_count = static_cast<int>(shadow.directional.cascade_count);
        if (ImGui::SliderInt("Cascades", &cascade_count, 1, KGPU_CSM_CASCADE_COUNT))
        {
            shadow.directional.cascade_count = static_cast<uint32_t>(cascade_count);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Number of Cascaded Shadow Map (CSM) splits.\n\n"
                "1 cascade: uniform resolution, crisp everywhere but limited range.\n"
                "4 cascades: high-res near, lower-res far (expected CSM trade-off).\n"
                "More cascades = more shadow passes = higher GPU cost.");
        }
        if (reset_button("D##cascades", "Reset to default: 4"))
        {
            shadow.directional.cascade_count = DEF_CASCADE_COUNT;
        }

        // Shadow distance
        float& shadow_dist = vr.get_shadow_distance();
        if (ImGui::DragFloat("Distance", &shadow_dist, 1.0f, 10.0f, 2000.0f, "%.0f m"))
        {
            shadow_dist = std::max(shadow_dist, 10.0f);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Maximum distance from camera where shadows are rendered.\n\n"
                "Lower: sharper shadows (cascades cover less area).\n"
                "Higher: shadows visible farther but each cascade is lower resolution.\n"
                "Ctrl+click to type exact value.");
        }
        if (reset_button("D##dist", "Reset to default: 100 m"))
        {
            shadow_dist = DEF_SHADOW_DISTANCE;
        }

        // Cascade split info
        if (ImGui::TreeNode("Cascade Splits"))
        {
            for (uint32_t i = 0; i < shadow.directional.cascade_count; ++i)
            {
                ImGui::BulletText(
                    "Cascade %u: %.1f m", i, shadow.directional.cascades[i].split_depth);
            }
            ImGui::TreePop();
        }

        ImGui::Spacing();
        ImGui::Separator();

        if (ImGui::Button("Reset All"))
        {
            shadow.directional.pcf_mode = DEF_PCF_MODE;
            shadow.directional.shadow_bias = DEF_SHADOW_BIAS;
            shadow.directional.normal_bias = DEF_NORMAL_BIAS;
            shadow.directional.cascade_count = DEF_CASCADE_COUNT;
            vr.get_shadow_distance() = DEF_SHADOW_DISTANCE;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Reset all shadow settings to defaults");
        }

        ImGui::SameLine();
        ImGui::TextDisabled("| %dx%d shadow maps", KGPU_SHADOW_MAP_SIZE, KGPU_SHADOW_MAP_SIZE);
    }

    // =========================================================================
    // Debug Lights
    // =========================================================================
    if (ImGui::CollapsingHeader("Debug Lights"))
    {
        auto& cfg = vr.get_debug_light_config();

        ImGui::Checkbox("Wireframe", &cfg.show_wireframe);
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Show wireframe shapes at light positions (radius, cone).");
        }

        ImGui::SameLine();

        ImGui::Checkbox("Icons", &cfg.show_icons);
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Show billboard icons at light positions.");
        }
    }

    handle_end();
}

}  // namespace ui
}  // namespace kryga