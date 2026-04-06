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
#include <vulkan_render/render_config.h>
#include <gpu_types/gpu_shadow_types.h>
#include <gpu_types/gpu_cluster_types.h>
#include <vfs/vfs.h>

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
    auto& cfg = vr.get_render_config();

    // Default config for reset comparisons
    const render::render_config defaults;

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

    // =========================================================================
    // Shadows
    // =========================================================================
    if (ImGui::CollapsingHeader("Shadows", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // PCF Mode
        const char* pcf_names[] = {
            "3x3 (9 taps)", "5x5 (25 taps)", "7x7 (49 taps)", "Poisson 16", "Poisson 32"};
        int pcf = static_cast<int>(cfg.shadows.pcf);
        if (ImGui::Combo("PCF Mode", &pcf, pcf_names, IM_ARRAYSIZE(pcf_names)))
        {
            cfg.shadows.pcf = static_cast<render::pcf_mode>(pcf);
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
            cfg.shadows.pcf = defaults.shadows.pcf;
        }

        // Bias
        if (ImGui::DragFloat(
                "Shadow Bias", &cfg.shadows.bias, 0.0001f, KGPU_SHADOW_BIAS_MIN, KGPU_SHADOW_BIAS_MAX, "%.4f"))
        {
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
            cfg.shadows.bias = defaults.shadows.bias;
        }

        // Normal Bias
        if (ImGui::DragFloat(
                "Normal Bias",
                &cfg.shadows.normal_bias,
                0.001f,
                KGPU_SHADOW_NORMAL_BIAS_MIN,
                KGPU_SHADOW_NORMAL_BIAS_MAX,
                "%.3f"))
        {
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
            cfg.shadows.normal_bias = defaults.shadows.normal_bias;
        }

        ImGui::Spacing();

        // Cascade count
        int cascade_count = static_cast<int>(cfg.shadows.cascade_count);
        if (ImGui::SliderInt(
                "Cascades", &cascade_count, KGPU_CSM_CASCADE_COUNT_MIN, KGPU_CSM_CASCADE_COUNT_MAX))
        {
            cfg.shadows.cascade_count = static_cast<uint32_t>(cascade_count);
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
            cfg.shadows.cascade_count = defaults.shadows.cascade_count;
        }

        // Shadow distance
        if (ImGui::DragFloat(
                "Distance",
                &cfg.shadows.distance,
                1.0f,
                KGPU_SHADOW_DISTANCE_MIN,
                KGPU_SHADOW_DISTANCE_MAX,
                "%.0f m"))
        {
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Maximum distance from camera where shadows are rendered.\n\n"
                "Lower: sharper shadows (cascades cover less area).\n"
                "Higher: shadows visible farther but each cascade is lower resolution.\n"
                "Ctrl+click to type exact value.");
        }
        if (reset_button("D##dist", "Reset to default: 200 m"))
        {
            cfg.shadows.distance = defaults.shadows.distance;
        }

        // Cascade split info
        if (ImGui::TreeNode("Cascade Splits"))
        {
            for (uint32_t i = 0; i < cfg.shadows.cascade_count; ++i)
            {
                ImGui::BulletText(
                    "Cascade %u: %.1f m", i, vr.get_cascade_split_depth(i));
            }
            ImGui::TreePop();
        }

        ImGui::Spacing();

        // Shadow map resolution
        const int sm_sizes[] = {256, 512, 1024, 2048, 4096, 8192};
        const char* sm_labels[] = {"256", "512", "1024", "2048", "4096", "8192"};
        int sm_current = 0;
        for (int i = 0; i < IM_ARRAYSIZE(sm_sizes); ++i)
        {
            if (static_cast<int>(cfg.shadows.map_size) == sm_sizes[i])
            {
                sm_current = i;
                break;
            }
        }
        if (ImGui::Combo("Shadow Map Size", &sm_current, sm_labels, IM_ARRAYSIZE(sm_labels)))
        {
            cfg.shadows.map_size = static_cast<uint32_t>(sm_sizes[sm_current]);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Resolution of each shadow map (NxN).\n\n"
                "Higher: sharper shadows, more VRAM.\n"
                "Lower: softer/blockier shadows, less VRAM.\n\n"
                "Changes trigger shadow pass rebuild.");
        }
        if (reset_button("D##smsize", "Reset to default: 2048"))
        {
            cfg.shadows.map_size = defaults.shadows.map_size;
        }

        ImGui::Spacing();
        ImGui::Separator();

        if (ImGui::Button("Reset Shadows"))
        {
            cfg.shadows = defaults.shadows;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Reset all shadow settings to defaults");
        }

        ImGui::SameLine();
        ImGui::TextDisabled("| %dx%d shadow maps", cfg.shadows.map_size, cfg.shadows.map_size);
    }

    // =========================================================================
    // Clusters
    // =========================================================================
    if (ImGui::CollapsingHeader("Clusters"))
    {
        int tile_size = static_cast<int>(cfg.clusters.tile_size);
        if (ImGui::SliderInt(
                "Tile Size",
                &tile_size,
                KGPU_cluster_tile_size_min,
                KGPU_cluster_tile_size_max))
        {
            cfg.clusters.tile_size = static_cast<uint32_t>(tile_size);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Screen-space tile size in pixels for cluster grid.\n\n"
                "Smaller: more clusters, finer light assignment, higher CPU/GPU cost.\n"
                "Larger: fewer clusters, coarser assignment, cheaper.");
        }
        if (reset_button("D##tile", "Reset to default"))
        {
            cfg.clusters.tile_size = defaults.clusters.tile_size;
        }

        int depth_slices = static_cast<int>(cfg.clusters.depth_slices);
        if (ImGui::SliderInt(
                "Depth Slices",
                &depth_slices,
                KGPU_cluster_depth_slices_min,
                KGPU_cluster_depth_slices_max))
        {
            cfg.clusters.depth_slices = static_cast<uint32_t>(depth_slices);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Number of logarithmic depth slices.\n\n"
                "More slices: tighter depth ranges per cluster, fewer false-positive lights.\n"
                "Fewer slices: more lights assigned to clusters they don't affect.");
        }
        if (reset_button("D##depth", "Reset to default"))
        {
            cfg.clusters.depth_slices = defaults.clusters.depth_slices;
        }

        int max_lights = static_cast<int>(cfg.clusters.max_lights_per_cluster);
        if (ImGui::SliderInt(
                "Max Lights/Cluster",
                &max_lights,
                KGPU_max_lights_per_cluster_min,
                KGPU_max_lights_per_cluster_max))
        {
            cfg.clusters.max_lights_per_cluster = static_cast<uint32_t>(max_lights);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Maximum number of lights that can affect a single cluster.\n\n"
                "Higher: handles dense light scenarios, uses more memory.\n"
                "Lower: saves memory, excess lights silently dropped.");
        }
        if (reset_button("D##maxl", "Reset to default"))
        {
            cfg.clusters.max_lights_per_cluster = defaults.clusters.max_lights_per_cluster;
        }

        ImGui::Spacing();
        ImGui::Separator();

        if (ImGui::Button("Reset Clusters"))
        {
            cfg.clusters = defaults.clusters;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Reset cluster settings to defaults");
        }

        ImGui::SameLine();
        const auto& grid = vr.get_render_config().clusters;
        ImGui::TextDisabled("| %ux%u tiles", grid.tile_size, grid.depth_slices);
    }

    // =========================================================================
    // Debug
    // =========================================================================
    if (ImGui::CollapsingHeader("Debug"))
    {
        ImGui::Checkbox("Grid", &cfg.debug.show_grid);
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Show ground grid.");
        }

        ImGui::SameLine();

        ImGui::Checkbox("Wireframe", &cfg.debug.light_wireframe);
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Show wireframe shapes at light positions (radius, cone).");
        }

        ImGui::SameLine();

        ImGui::Checkbox("Icons", &cfg.debug.light_icons);
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Show billboard icons at light positions.");
        }

        ImGui::SameLine();

        ImGui::Checkbox("Frustum Culling", &cfg.debug.frustum_culling);
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Enable frustum culling.\n\n"
                "Per-object mode: CPU-side sphere-frustum test.\n"
                "Instanced mode: always active (GPU compute).");
        }
    }

    // =========================================================================
    // Render Mode
    // =========================================================================
    if (ImGui::CollapsingHeader("Render Mode"))
    {
        const char* mode_names[] = {"Instanced", "Per Object"};
        int mode = static_cast<int>(cfg.mode);
        if (ImGui::Combo("Mode", &mode, mode_names, IM_ARRAYSIZE(mode_names)))
        {
            cfg.mode = static_cast<render::render_mode>(mode);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Instanced: batched drawing with GPU cluster + frustum culling.\n"
                "Per Object: legacy per-object drawing with CPU light grid.\n\n"
                "Switching triggers a render graph rebuild.");
        }
    }

    // =========================================================================
    // Save / Reset All
    // =========================================================================
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Save"))
    {
        auto rp =
            glob::glob_state().getr_vfs().real_path(vfs::rid("data://configs/render.acfg"));
        auto path = APATH(rp.value());
        cfg.save(path);
        render::render_config::delete_tmp(path);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Save current settings to render.acfg");
    }

    ImGui::SameLine();

    if (ImGui::Button("Reset All"))
    {
        auto mode = cfg.mode;  // Preserve render mode — requires reinit to change
        cfg = defaults;
        cfg.mode = mode;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Reset all settings to defaults (does not save to file)");
    }

    handle_end();
}

}  // namespace ui
}  // namespace kryga