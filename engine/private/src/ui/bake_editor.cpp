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
#include <packages/base/model/lights/components/point_light_component.h>
#include <packages/base/model/lights/components/spot_light_component.h>

#include <vulkan_render/bake/lightmap_baker.h>
#include <core/lightmap_manifest.h>
#include <vulkan_render/lightmap_atlas.h>
#include <vulkan_render/vulkan_render_loader.h>
#include <vulkan_render/kryga_render.h>
#include <vulkan_render/types/vulkan_texture_data.h>

#include <gpu_types/gpu_vertex_types.h>
#include <gpu_types/gpu_light_types.h>

#include <core/level.h>
#include <core/queues.h>
#include <vfs/vfs.h>
#include <vfs/io.h>

#include <utils/kryga_log.h>
#include <utils/buffer.h>

#include <imgui.h>

namespace kryga
{
namespace ui
{

void
bake_editor::init(const vfs::rid& base, const vfs::rid& cache)
{
    m_cache_rid = cache;
    m_config.load_with_cache(base, cache);
}

void
bake_editor::save_config()
{
    if (!m_cache_rid.empty())
    {
        m_config.save_to_cache(m_cache_rid);
    }
}

void
bake_editor::handle()
{
    if (!handle_begin())
    {
        return;
    }

    auto& actions = glob::glob_state().getr_ui().m_actions;
    auto& cfg = m_config;

    // --- Presets ---
    if (ImGui::CollapsingHeader("Presets", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Button("Low", ImVec2(80, 0)))
        {
            cfg.apply_preset(render::bake::bake_preset::low);
        }
        ImGui::SameLine();
        if (ImGui::Button("Medium", ImVec2(80, 0)))
        {
            cfg.apply_preset(render::bake::bake_preset::medium);
        }
        ImGui::SameLine();
        if (ImGui::Button("High", ImVec2(80, 0)))
        {
            cfg.apply_preset(render::bake::bake_preset::high);
        }
        ImGui::SameLine();
        if (ImGui::Button("Maximum", ImVec2(80, 0)))
        {
            cfg.apply_preset(render::bake::bake_preset::maximum);
        }
    }

    // --- Settings ---
    if (ImGui::CollapsingHeader("Bake Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        static const int resolutions[] = {256, 512, 1024, 2048, 4096};
        static const char* res_labels[] = {"256", "512", "1024", "2048", "4096"};
        int res_idx = 2;
        int cur_res = static_cast<int>(cfg.resolution);
        for (int i = 0; i < 5; ++i)
        {
            if (resolutions[i] == cur_res)
            {
                res_idx = i;
            }
        }
        if (ImGui::Combo("Resolution", &res_idx, res_labels, 5))
        {
            cfg.resolution = resolutions[res_idx];
        }

        int samples = static_cast<int>(cfg.samples_per_texel);
        int bounces = static_cast<int>(cfg.bounce_count);
        int denoise = static_cast<int>(cfg.denoise_iterations);
        ImGui::SliderInt("Samples/Texel", &samples, 1, 1024);
        ImGui::SliderInt("Bounces", &bounces, 0, 8);
        ImGui::SliderInt("Denoise Passes", &denoise, 0, 8);
        cfg.samples_per_texel = samples;
        cfg.bounce_count = bounces;
        cfg.denoise_iterations = denoise;

        ImGui::Separator();
        ImGui::Checkbox("Direct", &cfg.bake_direct);
        ImGui::SameLine();
        ImGui::Checkbox("Indirect", &cfg.bake_indirect);
        ImGui::SameLine();
        ImGui::Checkbox("AO", &cfg.bake_ao);
        if (cfg.bake_ao)
        {
            ImGui::DragFloat("AO Radius", &cfg.ao_radius, 0.1f, 0.1f, 50.0f);
            ImGui::DragFloat("AO Intensity", &cfg.ao_intensity, 0.05f, 0.0f, 5.0f);
        }
        ImGui::Separator();
        ImGui::DragFloat("Texels/Unit", &cfg.texels_per_unit, 0.5f, 0.5f, 32.0f);
        ImGui::SliderInt("Min Tile", &cfg.min_tile, 4, 128);
        ImGui::SliderInt("Max Tile", &cfg.max_tile, 32, 512);
        ImGui::Separator();

        int shadow_smp = static_cast<int>(cfg.shadow_samples);
        int dilate_iter = static_cast<int>(cfg.dilate_iterations);
        ImGui::SliderInt("Shadow Samples", &shadow_smp, 1, 128);
        ImGui::DragFloat("Shadow Spread", &cfg.shadow_spread, 0.001f, 0.0f, 0.05f, "%.4f");
        ImGui::DragFloat("Shadow Bias", &cfg.shadow_bias, 0.001f, 0.001f, 0.1f, "%.4f");
        ImGui::SliderInt("Dilate Passes", &dilate_iter, 0, 8);
        cfg.shadow_samples = shadow_smp;
        cfg.dilate_iterations = dilate_iter;

        ImGui::Separator();
        ImGui::Checkbox("Save PNG previews", &cfg.save_png);
    }

    // --- Scene info ---
    auto* level = glob::glob_state().get_current_level();
    int static_count = 0;
    int light_count = 0;
    int local_light_count = 0;

    if (level)
    {
        for (auto& [id, obj_ptr] : level->get_game_objects().get_items())
        {
            auto* go = static_cast<root::game_object*>(obj_ptr);
            for (auto* comp : go->get_renderable_components())
            {
                if (auto* mc = dynamic_cast<base::mesh_component*>(comp))
                {
                    if (mc->get_layers().contribute_gi)
                    {
                        ++static_count;
                    }
                }
                if (auto* dlc = dynamic_cast<base::directional_light_component*>(comp))
                {
                    if (dlc->get_selected())
                    {
                        ++light_count;
                    }
                }
                if (dynamic_cast<base::point_light_component*>(comp) ||
                    dynamic_cast<base::spot_light_component*>(comp))
                {
                    ++local_light_count;
                }
            }
        }
    }

    ImGui::Text("Static meshes: %d", static_count);
    ImGui::Text("Directional lights: %d", light_count);
    ImGui::Text("Local lights: %d", local_light_count);

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
            utils::id component_id;
            std::vector<gpu::vertex_data> vertices;
            std::vector<uint32_t> indices;
            glm::mat4 transform;
        };

        auto meshes = std::make_shared<std::vector<bake_mesh_data>>();
        auto lights = std::make_shared<std::vector<gpu::directional_light_data>>();
        auto local_lights = std::make_shared<std::vector<gpu::universal_light_data>>();

        for (auto& [id, obj_ptr] : level->get_game_objects().get_items())
        {
            auto* go = static_cast<root::game_object*>(obj_ptr);
            for (auto* comp : go->get_renderable_components())
            {
                if (auto* mc = dynamic_cast<base::mesh_component*>(comp))
                {
                    if (!mc->get_layers().contribute_gi || !mc->get_mesh())
                    {
                        continue;
                    }

                    auto& vert_buf = mc->get_mesh()->get_vertices_buffer();
                    auto& idx_buf = mc->get_mesh()->get_indices_buffer();
                    if (vert_buf.size() == 0 || idx_buf.size() == 0)
                    {
                        continue;
                    }

                    bake_mesh_data md;
                    md.component_id = mc->get_id();
                    auto* src_verts = reinterpret_cast<const gpu::vertex_data*>(vert_buf.data());
                    uint32_t vert_count =
                        static_cast<uint32_t>(vert_buf.size() / sizeof(gpu::vertex_data));
                    md.vertices.assign(src_verts, src_verts + vert_count);

                    // Debug: check UV2 range
                    float uv2_max = 0.0f;
                    for (uint32_t vi = 0; vi < vert_count; ++vi)
                    {
                        uv2_max = std::max(
                            uv2_max,
                            std::max(std::abs(src_verts[vi].uv2.x), std::abs(src_verts[vi].uv2.y)));
                    }
                    ALOG_INFO("bake_editor: {} verts={} uv2_max={:.4f}",
                              mc->get_id().str(),
                              vert_count,
                              uv2_max);

                    auto* src_indices = reinterpret_cast<const uint32_t*>(idx_buf.data());
                    uint32_t idx_count = static_cast<uint32_t>(idx_buf.size() / sizeof(uint32_t));
                    md.indices.assign(src_indices, src_indices + idx_count);

                    md.transform = mc->get_transform_matrix();
                    meshes->push_back(std::move(md));
                }

                if (auto* dlc = dynamic_cast<base::directional_light_component*>(comp))
                {
                    // Only use the selected directional light for baking
                    if (!dlc->get_selected())
                    {
                        continue;
                    }

                    gpu::directional_light_data ld{};
                    ld.direction = dlc->get_direction();
                    ld.ambient = dlc->get_ambient();
                    ld.diffuse = dlc->get_diffuse();
                    ld.specular = dlc->get_specular();
                    lights->push_back(ld);
                }

                if (auto* plc = dynamic_cast<base::point_light_component*>(comp))
                {
                    gpu::universal_light_data ld{};
                    ld.position = glm::vec3(plc->get_world_position());
                    ld.ambient = plc->get_ambient();
                    ld.diffuse = plc->get_diffuse();
                    ld.specular = plc->get_specular();
                    ld.radius = plc->get_radius();
                    ld.type = KGPU_light_type_point;
                    ld.cut_off = -1.0f;
                    ld.outer_cut_off = -1.0f;
                    local_lights->push_back(ld);
                }

                if (auto* slc = dynamic_cast<base::spot_light_component*>(comp))
                {
                    gpu::universal_light_data ld{};
                    ld.position = glm::vec3(slc->get_world_position());
                    ld.direction = slc->get_direction();
                    ld.ambient = slc->get_ambient();
                    ld.diffuse = slc->get_diffuse();
                    ld.specular = slc->get_specular();
                    ld.radius = slc->get_radius();
                    ld.type = KGPU_light_type_spot;
                    ld.cut_off = std::cos(glm::radians(slc->get_cut_off()));
                    ld.outer_cut_off = std::cos(glm::radians(slc->get_outer_cut_off()));
                    local_lights->push_back(ld);
                }
            }
        }

        // Capture config by value and save state for next session
        auto bake_cfg = m_config;
        save_config();

        engine::action a;
        a.name = "Bake Lightmaps";
        a.work = [=](engine::action_progress& progress)
        {
            progress.set_status("Packing atlas...");
            progress.progress.store(0.05f);

            uint32_t resolution = bake_cfg.resolution;
            float texels_per_unit = bake_cfg.texels_per_unit;
            int min_tile = bake_cfg.min_tile;
            int max_tile = bake_cfg.max_tile;

            render::lightmap_atlas atlas(resolution, resolution);
            render::lightmap_baker baker;
            core::lightmap_manifest manifest;
            manifest.atlas_width = resolution;
            manifest.atlas_height = resolution;

            float inv_w = 1.0f / float(resolution);
            float inv_h = 1.0f / float(resolution);

            for (size_t i = 0; i < meshes->size(); ++i)
            {
                auto& md = (*meshes)[i];
                auto tile_id = AID(("bake_mesh_" + std::to_string(i)).c_str());

                // Compute tile size from object bounding size * texels_per_unit
                glm::vec3 bb_min(std::numeric_limits<float>::max());
                glm::vec3 bb_max(std::numeric_limits<float>::lowest());
                for (auto& v : md.vertices)
                {
                    auto wp = glm::vec3(md.transform * glm::vec4(v.position, 1.0f));
                    bb_min = glm::min(bb_min, wp);
                    bb_max = glm::max(bb_max, wp);
                }
                float max_extent = glm::max(bb_max.x - bb_min.x,
                                            glm::max(bb_max.y - bb_min.y, bb_max.z - bb_min.z));
                uint32_t tile_size = static_cast<uint32_t>(std::ceil(max_extent * texels_per_unit));
                tile_size = std::clamp(
                    tile_size, static_cast<uint32_t>(min_tile), static_cast<uint32_t>(max_tile));
                // Round up to multiple of 4 for GPU alignment
                tile_size = (tile_size + 3) & ~3u;

                // Allocate with padding so dilation fills the gutter between tiles
                constexpr uint32_t padding = 4;
                uint32_t padded_size = tile_size + padding * 2;

                if (!atlas.allocate(tile_id, padded_size, padded_size))
                {
                    ALOG_WARN("bake_editor: atlas full at mesh {}, skipping remaining", i);
                    break;
                }

                auto* region = atlas.get_region(tile_id);

                // UV maps to inner region (excluding padding border)
                std::vector<gpu::vertex_data> remapped = md.vertices;
                glm::vec2 scale = {float(tile_size) * inv_w, float(tile_size) * inv_h};
                glm::vec2 offset = {float(region->x + padding) * inv_w,
                                    float(region->y + padding) * inv_h};

                for (auto& v : remapped)
                {
                    v.position = glm::vec3(md.transform * glm::vec4(v.position, 1.0f));
                    v.normal = glm::normalize(glm::mat3(md.transform) * v.normal);
                    v.uv2.x = v.uv2.x * scale.x + offset.x;
                    v.uv2.y = v.uv2.y * scale.y + offset.y;
                }

                baker.add_mesh(remapped.data(),
                               static_cast<uint32_t>(remapped.size()),
                               md.indices.data(),
                               static_cast<uint32_t>(md.indices.size()));

                // Record in manifest (inner region, excluding padding)
                core::lightmap_object_entry entry;
                entry.region_x = region->x + padding;
                entry.region_y = region->y + padding;
                entry.region_w = tile_size;
                entry.region_h = tile_size;
                entry.lightmap_scale = scale;
                entry.lightmap_offset = offset;
                manifest.objects[md.component_id] = entry;
            }

            progress.set_status("Baking (GPU compute)...");
            progress.progress.store(0.1f);

            // Save to level's baked/ directory
            auto* cur_level = glob::glob_state().get_current_level();
            KRG_check(cur_level, "No level loaded during bake");
            auto baked_root = cur_level->get_vfs_root() / "baked";

            render::bake::bake_settings settings;
            static_cast<render::bake::bake_config&>(settings) = bake_cfg;
            settings.directional_lights = *lights;
            settings.local_lights = *local_lights;
            settings.output_lightmap = baked_root / "lightmap.bin";
            settings.output_ao = baked_root / "ao.bin";
            settings.output_png = bake_cfg.save_png;

            auto result = baker.bake(settings);

            if (!result.success)
            {
                throw std::runtime_error("Lightmap bake failed");
            }

            progress.set_status("Saving manifest...");
            progress.progress.store(0.9f);

            // Save manifest via VFS
            manifest.save(baked_root / "lightmap_manifest.cfg");

            progress.set_status("Loading into renderer...");
            progress.progress.store(0.95f);

            // Load the baked lightmap texture into the renderer (must happen on this thread
            // since immediate_submit needs the device). The level will pick it up on next
            // object rebuild.
            const auto& lm_data = baker.get_lightmap_data();
            if (!lm_data.empty() && cur_level)
            {
                utils::buffer lm_buf(lm_data.size());
                std::memcpy(lm_buf.data(), lm_data.data(), lm_data.size());

                auto& loader = glob::glob_state().getr_vulkan_render_loader();

                auto lm_tex_id = AID((cur_level->get_id().str() + "_lightmap").c_str());

                auto* tex = loader.update_or_create_texture(lm_tex_id,
                                                            lm_buf,
                                                            result.atlas_width,
                                                            result.atlas_height,
                                                            VK_FORMAT_R16G16B16A16_SFLOAT,
                                                            render::texture_format::rgba16f);

                if (tex)
                {
                    // Set references so root.cfg persists them on save
                    cur_level->set_lightmap_refs(baked_root / "lightmap.bin",
                                                 baked_root / "lightmap_manifest.cfg");
                    cur_level->set_lightmap(
                        tex->get_bindless_index(),
                        std::make_unique<core::lightmap_manifest>(std::move(manifest)));

                    // Mark all static mesh components as render-dirty so they pick up
                    // lightmap data through cmd_builder on next frame
                    for (auto& [oid, obj_ptr] : cur_level->get_game_objects().get_items())
                    {
                        auto* go = static_cast<root::game_object*>(obj_ptr);
                        for (auto* comp : go->get_renderable_components())
                        {
                            if (comp->get_layers().contribute_gi)
                            {
                                glob::glob_state()
                                    .getr_queues()
                                    .get_model()
                                    .dirty_render.emplace_back(comp);
                            }
                        }
                    }
                }
            }

            progress.progress.store(1.0f);
            progress.set_status("Done");

            ALOG_INFO("bake_editor: bake complete — {}x{}, {} tris, {:.0f}ms",
                      result.atlas_width,
                      result.atlas_height,
                      result.total_triangles,
                      result.bake_time_ms);
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
