#include "engine/ui.h"

#include <global_state/global_state.h>

#include "engine/console.h"
#include "engine/engine_counters.h"

#include "engine/private/ui/gizmo_editor.h"
#include "engine/private/ui/material_previewer.h"
#include "engine/private/ui/bake_editor.h"
#include "engine/private/ui/converter_window.h"
#include "engine/editor.h"
#include "engine/editor_system.h"

#include <native/native_window.h>

#include <vulkan_render/kryga_render.h>
#include <vulkan_render/render_config.h>
#include <vulkan_render/render_system.h>
#include <gpu_types/gpu_shadow_types.h>
#include <gpu_types/gpu_cluster_types.h>
#include <vfs/vfs.h>

#include <SDL.h>
#include <SDL_vulkan.h>

#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_vulkan.h>

#include <ImGuizmo.h>

#include <algorithm>

namespace kryga
{

namespace ui
{

ui::ui()
    : m_gizmo_editor(std::make_unique<gizmo_editor>())
    , m_material_previewer(std::make_unique<material_previewer>())
{
    m_windows[performance_counters_window::window_title()] =
        std::make_unique<performance_counters_window>();
    m_windows[performance_counters_window::window_title()]->m_show = true;

    m_windows[render_config_window::window_title()] = std::make_unique<render_config_window>();
    m_windows[render_config_window::window_title()]->m_show = true;

    m_windows[bake_editor::window_title()] = std::make_unique<bake_editor>();
    m_windows[converter_window::window_title()] = std::make_unique<converter_window>();
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

    // Persist window layout to rtcache directory
    auto& vfs = glob::glob_state().getr_vfs();
    vfs.create_directories(vfs::rid("rtcache://"));
    auto ini_rp = vfs.real_path(vfs::rid("rtcache://imgui.ini"));
    if (ini_rp.has_value())
    {
        m_imgui_ini_path = ini_rp.value().string();
        io.IniFilename = m_imgui_ini_path.c_str();
    }

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
    m_actions.tick();

    auto* conv = get_window<converter_window>();
    if (conv)
    {
        conv->poll();
    }

    ImGuiIO& io = ImGui::GetIO();
    // SDL2 backend resets DisplaySize from SDL_GetWindowSize inside
    // ImGui_ImplSDL2_NewFrame — override before AND after that call.
    auto& vr = glob::glob_state().getr_render().renderer;
    io.DisplaySize = ImVec2((float)vr.width(), (float)vr.height());
    io.DeltaTime = dt;

    ImGui_ImplSDL2_NewFrame();
    io.DisplaySize = ImVec2((float)vr.width(), (float)vr.height());
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

    ImGui::NewFrame();

    // Fullscreen dockspace — all windows can dock into this, central node is transparent for 3D
    // viewport
    ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

    bool playing =
        glob::glob_state().getr_editor_system().editor.get_mode() == engine::editor_mode::playing;

    if (playing)
    {
        auto it = m_windows.find(performance_counters_window::window_title());
        if (it != m_windows.end())
        {
            it->second->handle();
        }

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

    // Console overlay renders on top in all modes
    if (auto* console = editor_console::instance())
    {
        console->handle();
    }

    ImGuizmo::BeginFrame();

    m_gizmo_editor->draw();

    glob::glob_state().getr_editor_system().screenshot.draw_overlay();

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

        auto mem = glob::glob_state().getr_render().device.get_memory_stats();
        auto mb = [](uint64_t b) { return static_cast<double>(b) / (1024.0 * 1024.0); };
        vram_used_mb = mb(mem.device_used);
        vram_total_mb = mb(mem.device_total);
        host_used_mb = mb(mem.host_used);
        vma_alloc_count = mem.allocation_count;

        lock = 24;
    }

    ImGui::Separator();
    ImGui::Text("Frame   : %3.3lf", frame_avg);
    ImGui::Text("FPS     : %3.3lf", fps);
    ImGui::Text("Input   : %3.3lf", input_avg);
    {
        auto& dev = glob::glob_state().getr_render().device;
        if (dev.present_wait_supported())
        {
            ImGui::Text("Display : %3.3f ms", dev.present_latency_ms());
        }
        else
        {
            ImGui::TextDisabled("Display : n/a (no present_wait)");
        }
    }
    ImGui::Separator();
    ImGui::Text("Tick    : %3.3lf", tick_avg);
    ImGui::Text("UI tick : %3.3lf", ui_tick_avg);
    ImGui::Text("Update  : %3.3lf", consume_updates_avg);
    ImGui::Separator();
    ImGui::Text("Draw    : %3.3lf", draw_avg);
    ImGui::Separator();
    ImGui::Text("Objects : %3.3lf", objects_avg);
    ImGui::Text("Draws   : %3.3lf", all_draws_avg);
    ImGui::Text("Cull %  : %3.3lf",
                all_draws_avg > 0 ? culled_draws_avg / all_draws_avg * 100 : 0.0);
    ImGui::Separator();
    ImGui::Text("VRAM    : %.1f / %.0f MB", vram_used_mb, vram_total_mb);
    ImGui::Text("Host mem: %.1f MB", host_used_mb);
    ImGui::Text("Allocs  : %u", vma_alloc_count);
    ImGui::Separator();

    --lock;
}

void
render_config_window::handle()
{
    if (!handle_begin())
    {
        return;
    }

    auto& vr = glob::glob_state().getr_render().renderer;
    // Mutate pending — apply_pending_render_config() picks it up between frames.
    auto& cfg = vr.get_pending_render_config();

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
    if (ImGui::CollapsingHeader("Lighting"))
    {
        ImGui::Checkbox("Directional Light", &cfg.lighting.directional_enabled);
        ImGui::Checkbox("Local Lights (Point/Spot)", &cfg.lighting.local_enabled);
        ImGui::Checkbox("Baked Light (Lightmaps)", &cfg.lighting.baked_enabled);
    }

    if (ImGui::CollapsingHeader("Shadows", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Shadows Enabled", &cfg.shadows.enabled);

        // PCF Mode
        static const char* pcf_labels[render::pcf_mode_count];
        static bool pcf_labels_init = [&]
        {
            for (int i = 0; i < render::pcf_mode_count; ++i)
            {
                pcf_labels[i] = render::pcf_mode_entries[i].label;
            }
            return true;
        }();
        (void)pcf_labels_init;
        int pcf = static_cast<int>(cfg.shadows.pcf);
        if (ImGui::Combo("PCF Mode", &pcf, pcf_labels, render::pcf_mode_count))
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
        if (ImGui::DragFloat("Shadow Bias",
                             &cfg.shadows.bias,
                             0.0001f,
                             KGPU_SHADOW_BIAS_MIN,
                             KGPU_SHADOW_BIAS_MAX,
                             "%.4f"))
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
        if (ImGui::DragFloat("Normal Bias",
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

        if (ImGui::DragFloat("PCF Radius",
                             &cfg.shadows.pcf_world_radius,
                             0.001f,
                             0.001f,
                             1.0f,
                             "%.3f"))
        {
        }
        if (reset_button("D##pcfrad", "Reset to default: 0.030"))
        {
            cfg.shadows.pcf_world_radius = defaults.shadows.pcf_world_radius;
        }

        ImGui::Checkbox("HW PCF (CSM)", &cfg.shadows.hardware_pcf);
        ImGui::SameLine();
        ImGui::Checkbox("HW PCF (Local)", &cfg.shadows.hardware_pcf_local);

        ImGui::Spacing();

        int max_cascades = static_cast<int>(cfg.shadows.max_cascades());
        if (static_cast<int>(cfg.shadows.cascade_count) > max_cascades)
            cfg.shadows.cascade_count = static_cast<uint32_t>(max_cascades);
        int cascade_count = static_cast<int>(cfg.shadows.cascade_count);
        if (ImGui::SliderInt(
                "Cascades", &cascade_count, KGPU_CSM_CASCADE_COUNT_MIN, max_cascades))
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
        if (ImGui::DragFloat("Distance",
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
                ImGui::BulletText("Cascade %u: %.1f m", i, vr.get_cascade_split_depth(i));
            }
            ImGui::TreePop();
        }

        ImGui::Spacing();

        // Shadow atlas configuration
        const int atlas_sizes[] = {1024, 2048, 4096, 8192, 16384};
        const char* atlas_labels[] = {"1024", "2048", "4096", "8192", "16384"};
        int atlas_current = 2;
        for (int i = 0; i < IM_ARRAYSIZE(atlas_sizes); ++i)
        {
            if (static_cast<int>(cfg.shadows.atlas_size) == atlas_sizes[i])
            {
                atlas_current = i;
                break;
            }
        }
        if (ImGui::Combo("Atlas Size", &atlas_current, atlas_labels, IM_ARRAYSIZE(atlas_labels)))
        {
            cfg.shadows.atlas_size = static_cast<uint32_t>(atlas_sizes[atlas_current]);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Shadow atlas texture size (NxN).\nAll shadow maps are packed into this atlas.");
        }

        const int tile_sizes[] = {64, 128, 256, 512, 1024, 2048, 4096};
        const char* tile_labels[] = {"64", "128", "256", "512", "1024", "2048", "4096"};
        const int tile_count = IM_ARRAYSIZE(tile_sizes);
        uint32_t csm_limit = cfg.shadows.max_csm_tile();
        uint32_t local_limit = cfg.shadows.max_local_tile();

        // CSM Tile Size
        {
            int csm_current = 4;
            for (int i = 0; i < tile_count; ++i)
                if (static_cast<int>(cfg.shadows.csm_tile_size) == tile_sizes[i])
                { csm_current = i; break; }

            if (ImGui::BeginCombo("CSM Tile Size", tile_labels[csm_current]))
            {
                for (int i = 0; i < tile_count; ++i)
                {
                    uint32_t t = static_cast<uint32_t>(tile_sizes[i]);
                    bool too_large = t > csm_limit;
                    if (too_large) ImGui::BeginDisabled();
                    if (ImGui::Selectable(tile_labels[i], i == csm_current))
                        cfg.shadows.csm_tile_size = t;
                    if (too_large) ImGui::EndDisabled();
                }
                ImGui::EndCombo();
            }
        }

        // Local Tile Size
        {
            int local_current = 3;
            for (int i = 0; i < tile_count; ++i)
                if (static_cast<int>(cfg.shadows.local_tile_size) == tile_sizes[i])
                { local_current = i; break; }

            if (ImGui::BeginCombo("Local Tile Size", tile_labels[local_current]))
            {
                for (int i = 0; i < tile_count; ++i)
                {
                    bool too_large = static_cast<uint32_t>(tile_sizes[i]) > local_limit;
                    if (too_large) ImGui::BeginDisabled();
                    if (ImGui::Selectable(tile_labels[i], i == local_current))
                        cfg.shadows.local_tile_size = static_cast<uint32_t>(tile_sizes[i]);
                    if (too_large) ImGui::EndDisabled();
                }
                ImGui::EndCombo();
            }
        }

        {
            int local_lights = static_cast<int>(cfg.shadows.max_local_lights);
            if (ImGui::SliderInt(
                    "Shadow Lights", &local_lights, 0, KGPU_MAX_SHADOWED_LOCAL_LIGHTS))
            {
                cfg.shadows.max_local_lights = static_cast<uint32_t>(local_lights);
            }
        }

        ImGui::Checkbox("Depth 16-bit", &cfg.shadows.depth_16bit);

        if (reset_button("D##smsize", "Reset to defaults"))
        {
            cfg.shadows.atlas_size = defaults.shadows.atlas_size;
            cfg.shadows.csm_tile_size = defaults.shadows.csm_tile_size;
            cfg.shadows.local_tile_size = defaults.shadows.local_tile_size;
            cfg.shadows.depth_16bit = defaults.shadows.depth_16bit;
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

        uint32_t atlas_mb = (cfg.shadows.atlas_size / 1024) * (cfg.shadows.atlas_size / 1024) * 4 * 3;
        ImGui::SameLine();
        ImGui::TextDisabled("| atlas %dx%d (%u MB)", cfg.shadows.atlas_size, cfg.shadows.atlas_size, atlas_mb);
    }

    // =========================================================================
    // Clusters
    // =========================================================================
    if (ImGui::CollapsingHeader("Clusters"))
    {
        int tile_size = static_cast<int>(cfg.clusters.tile_size);
        if (ImGui::SliderInt(
                "Tile Size", &tile_size, KGPU_cluster_tile_size_min, KGPU_cluster_tile_size_max))
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
        if (ImGui::SliderInt("Depth Slices",
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
        if (ImGui::SliderInt("Max Lights/Cluster",
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
        ImGui::Checkbox("Editor Mode", &cfg.debug.editor_mode);
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Master toggle for editor-only visuals (grid, gizmos, debug "
                "wireframes, light icon billboards). Off = preview as game.");
        }

        ImGui::SameLine();

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
    // Render Scale
    // =========================================================================
    if (ImGui::CollapsingHeader("Render Scale"))
    {
        ImGui::Checkbox("Enabled##render_scale", &cfg.render_scale.enabled);
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Render scene at lower resolution then nearest-upscale.");
        }

        int divisor = (int)cfg.render_scale.divisor;
        if (ImGui::SliderInt("Divisor##render_scale", &divisor, 1, 10) && divisor >= 1)
        {
            cfg.render_scale.divisor = (uint32_t)divisor;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Scene is rendered at window_size / divisor, then nearest-upscaled.\n"
                "1 = native resolution. Higher = chunkier pixels.");
        }
    }

    // =========================================================================
    // Frames in Flight
    // =========================================================================
    if (ImGui::CollapsingHeader("Frames in Flight"))
    {
        auto& device = glob::glob_state().getr_render().device;

        // Present mode first — it determines the valid frame-count range below.
        static const char* present_labels[render::present_mode_count];
        for (int i = 0; i < render::present_mode_count; ++i)
        {
            present_labels[i] = render::present_mode_entries[i].label;
        }
        int present = static_cast<int>(cfg.present);
        if (ImGui::Combo("Present Mode", &present, present_labels, render::present_mode_count))
        {
            auto picked = static_cast<render::present_mode>(present);
            // Ignore modes the surface doesn't support — the combo selection
            // reverts next frame because cfg.present is left unchanged.
            if (device.is_present_mode_supported(picked))
            {
                cfg.present = picked;
                // Switching modes snaps Frames to the new mode's lowest valid
                // count, so we never sit on a value it can't honor.
                cfg.frames_in_flight = device.present_mode_image_range(picked).min;
            }
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "FIFO = vsync-capped (low power). Mailbox = low-latency uncapped\n"
                "framerate, but needs >=3 images. Switching modes snaps Frames to\n"
                "the new mode's minimum.");
        }

        // Frame count, bounded to the device's allowed range for the current
        // mode. Committed only on release: each commit is a full swapchain
        // rebuild, so dragging must NOT apply per intermediate value. While the
        // slider is held, fif_edit floats free; otherwise it tracks the config.
        auto range = device.present_mode_image_range(cfg.present);
        static int fif_edit = (int)cfg.frames_in_flight;
        static bool fif_dragging = false;
        if (!fif_dragging)
        {
            fif_edit = std::clamp((int)cfg.frames_in_flight, (int)range.min, (int)range.max);
        }
        ImGui::SliderInt("Frames##fif", &fif_edit, (int)range.min, (int)range.max);
        fif_dragging = ImGui::IsItemActive();
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            cfg.frames_in_flight = (uint32_t)fif_edit;  // commit -> single rebuild
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "How many frames the CPU runs ahead of the GPU. Range is what the\n"
                "device allows for the selected present mode. Applies on release\n"
                "(one swapchain rebuild). Lower = less GPU memory.");
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
        auto rp = glob::glob_state().getr_vfs().real_path(vfs::rid("data://configs/render.acfg"));
        auto path = APATH(rp.value());
        cfg.save(path);
        glob::glob_state().getr_vfs().remove(vfs::rid("rtcache://render.acfg"));
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Save current settings to render.acfg");
    }

    ImGui::SameLine();

    if (ImGui::Button("Reset All"))
    {
        cfg = defaults;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Reset all settings to defaults (does not save to file)");
    }

    handle_end();
}

}  // namespace ui
}  // namespace kryga
