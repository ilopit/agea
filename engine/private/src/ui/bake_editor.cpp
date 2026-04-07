#include "engine/private/ui/bake_editor.h"

#include <global_state/global_state.h>

#include <core/level.h>
#include <core/level_manager.h>
#include <core/caches/caches_map.h>

#include <packages/root/model/game_object.h>
#include <packages/root/model/assets/mesh.h>
#include <packages/root/model/assets/material.h>
#include <packages/base/model/components/mesh_component.h>
#include <packages/base/model/lights/components/directional_light_component.h>

#include <vulkan_render/bake/lightmap_baker.h>
#include <vulkan_render/lightmap_atlas.h>
#include <vulkan_render/vulkan_render_loader.h>
#include <vulkan_render/kryga_render.h>

#include <gpu_types/gpu_vertex_types.h>
#include <gpu_types/gpu_light_types.h>

#include <vfs/io.h>

#include <utils/kryga_log.h>

#include <imgui.h>

namespace kryga
{
namespace ui
{

void
bake_editor::handle()
{
    if (!handle_begin())
    {
        return;
    }

    auto& actions = glob::glob_state().getr_ui().m_actions;

    // --- Settings ---
    if (ImGui::CollapsingHeader("Bake Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        static const int resolutions[] = {256, 512, 1024, 2048, 4096};
        static const char* res_labels[] = {"256", "512", "1024", "2048", "4096"};
        int res_idx = 2;
        for (int i = 0; i < 5; ++i)
        {
            if (resolutions[i] == m_resolution)
                res_idx = i;
        }
        if (ImGui::Combo("Resolution", &res_idx, res_labels, 5))
        {
            m_resolution = resolutions[res_idx];
        }

        ImGui::SliderInt("Samples/Texel", &m_samples, 1, 256);
        ImGui::SliderInt("Bounces", &m_bounces, 0, 8);
        ImGui::SliderInt("Denoise Passes", &m_denoise, 0, 8);
        ImGui::Separator();
        ImGui::Checkbox("Direct", &m_bake_direct);
        ImGui::SameLine();
        ImGui::Checkbox("Indirect", &m_bake_indirect);
        ImGui::SameLine();
        ImGui::Checkbox("AO", &m_bake_ao);
        if (m_bake_ao)
        {
            ImGui::DragFloat("AO Radius", &m_ao_radius, 0.1f, 0.1f, 50.0f);
            ImGui::DragFloat("AO Intensity", &m_ao_intensity, 0.05f, 0.0f, 5.0f);
        }
        ImGui::Separator();
        ImGui::Checkbox("Save PNG previews", &m_save_png);
    }

    // --- Scene info ---
    auto* level = glob::glob_state().get_current_level();
    int static_count = 0;
    int light_count = 0;

    if (level)
    {
        for (auto& [id, obj_ptr] : level->get_game_objects().get_items())
        {
            auto* go = static_cast<root::game_object*>(obj_ptr);
            for (auto* comp : go->get_renderable_components())
            {
                if (auto* mc = dynamic_cast<base::mesh_component*>(comp))
                {
                    if (mc->get_static())
                        ++static_count;
                }
                if (dynamic_cast<base::directional_light_component*>(comp))
                {
                    ++light_count;
                }
            }
        }
    }

    ImGui::Text("Static meshes: %d", static_count);
    ImGui::Text("Directional lights: %d", light_count);

    // --- Bake button ---
    bool can_bake = level && static_count > 0 && light_count > 0 && !actions.is_busy();

    if (!can_bake)
    {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button("Bake Lightmaps", ImVec2(-1, 30)))
    {
        // Collect data on main thread (model objects are not thread-safe)
        struct bake_mesh_data
        {
            std::vector<gpu::vertex_data> vertices;
            std::vector<uint32_t> indices;
            glm::mat4 transform;
        };

        auto meshes = std::make_shared<std::vector<bake_mesh_data>>();
        auto lights = std::make_shared<std::vector<gpu::directional_light_data>>();

        for (auto& [id, obj_ptr] : level->get_game_objects().get_items())
        {
            auto* go = static_cast<root::game_object*>(obj_ptr);
            for (auto* comp : go->get_renderable_components())
            {
                if (auto* mc = dynamic_cast<base::mesh_component*>(comp))
                {
                    if (!mc->get_static() || !mc->get_mesh())
                        continue;

                    auto& vert_buf = mc->get_mesh()->get_vertices_buffer();
                    auto& idx_buf = mc->get_mesh()->get_indices_buffer();
                    if (vert_buf.size() == 0 || idx_buf.size() == 0)
                        continue;

                    bake_mesh_data md;
                    auto* src_verts =
                        reinterpret_cast<const gpu::vertex_data*>(vert_buf.data());
                    uint32_t vert_count =
                        static_cast<uint32_t>(vert_buf.size() / sizeof(gpu::vertex_data));
                    md.vertices.assign(src_verts, src_verts + vert_count);

                    auto* src_indices =
                        reinterpret_cast<const uint32_t*>(idx_buf.data());
                    uint32_t idx_count =
                        static_cast<uint32_t>(idx_buf.size() / sizeof(uint32_t));
                    md.indices.assign(src_indices, src_indices + idx_count);

                    md.transform = mc->get_transform_matrix();
                    meshes->push_back(std::move(md));
                }

                if (auto* dlc = dynamic_cast<base::directional_light_component*>(comp))
                {
                    gpu::directional_light_data ld{};
                    ld.direction = dlc->get_direction();
                    ld.ambient = dlc->get_ambient();
                    ld.diffuse = dlc->get_diffuse();
                    ld.specular = dlc->get_specular();
                    lights->push_back(ld);
                }
            }
        }

        // Capture settings by value
        int resolution = m_resolution;
        int samples = m_samples;
        int bounces = m_bounces;
        int denoise = m_denoise;
        float ao_radius = m_ao_radius;
        float ao_intensity = m_ao_intensity;
        bool bake_direct = m_bake_direct;
        bool bake_indirect = m_bake_indirect;
        bool bake_ao = m_bake_ao;
        bool save_png = m_save_png;

        engine::action a;
        a.name = "Bake Lightmaps";
        a.work = [=](engine::action_progress& progress)
        {
            progress.set_status("Packing atlas...");
            progress.progress.store(0.05f);

            uint32_t tile_size = std::max(8u, static_cast<uint32_t>(resolution) / 32);
            render::lightmap_atlas atlas(resolution, resolution);
            render::lightmap_baker baker;

            float inv_w = 1.0f / float(resolution);
            float inv_h = 1.0f / float(resolution);

            for (size_t i = 0; i < meshes->size(); ++i)
            {
                auto& md = (*meshes)[i];
                auto tile_id = AID(("bake_mesh_" + std::to_string(i)).c_str());

                if (!atlas.allocate(tile_id, tile_size, tile_size))
                {
                    ALOG_WARN("bake_editor: atlas full at mesh {}, skipping remaining", i);
                    break;
                }

                auto* region = atlas.get_region(tile_id);

                // Transform vertices to world space and remap UV2 to atlas
                std::vector<gpu::vertex_data> remapped = md.vertices;
                glm::vec2 scale = {float(region->width) * inv_w,
                                   float(region->height) * inv_h};
                glm::vec2 offset = {float(region->x) * inv_w,
                                    float(region->y) * inv_h};

                for (auto& v : remapped)
                {
                    v.position =
                        glm::vec3(md.transform * glm::vec4(v.position, 1.0f));
                    v.normal = glm::normalize(
                        glm::mat3(md.transform) * v.normal);
                    v.uv2.x = v.uv2.x * scale.x + offset.x;
                    v.uv2.y = v.uv2.y * scale.y + offset.y;
                }

                baker.add_mesh(remapped.data(),
                               static_cast<uint32_t>(remapped.size()),
                               md.indices.data(),
                               static_cast<uint32_t>(md.indices.size()));
            }

            progress.set_status("Baking (GPU compute)...");
            progress.progress.store(0.1f);

            render::bake::bake_settings cfg;
            cfg.resolution = resolution;
            cfg.samples_per_texel = samples;
            cfg.bounce_count = bounces;
            cfg.denoise_iterations = denoise;
            cfg.ao_radius = ao_radius;
            cfg.ao_intensity = ao_intensity;
            cfg.bake_direct = bake_direct;
            cfg.bake_indirect = bake_indirect;
            cfg.bake_ao = bake_ao;
            cfg.directional_lights = *lights;
            cfg.output_lightmap = vfs::rid("tmp://baked/lightmap.bin");
            cfg.output_ao = vfs::rid("tmp://baked/ao.bin");
            cfg.output_png = save_png;

            auto result = baker.bake(cfg);

            progress.progress.store(0.95f);
            progress.set_status("Done");

            if (!result.success)
            {
                throw std::runtime_error("Lightmap bake failed");
            }

            progress.progress.store(1.0f);

            ALOG_INFO("bake_editor: bake complete — {}x{}, {} tris, {:.0f}ms",
                      result.atlas_width, result.atlas_height,
                      result.total_triangles, result.bake_time_ms);
        };

        actions.submit(std::move(a));
    }

    if (!can_bake)
    {
        ImGui::EndDisabled();
    }

    if (!level)
    {
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "No level loaded");
    }
    else if (static_count == 0)
    {
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "No static meshes in level");
    }
    else if (light_count == 0)
    {
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "No directional lights in level");
    }

    handle_end();
}

}  // namespace ui
}  // namespace kryga
