#include "engine/private/ui/bake_editor.h"

#include <global_state/global_state.h>
#include <engine/editor_system.h>

#include <core/level.h>
#include <core/level_manager.h>
#include <core/model_system.h>
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
#include <vulkan_render/render_system.h>
#include <vulkan_render/types/vulkan_texture_data.h>
#include <render_bridge/render_translate.h>

#include <gpu_types/gpu_vertex_types.h>
#include <gpu_types/gpu_light_types.h>

#include <core/level.h>
#include <core/model_output.h>
#include <vfs/vfs.h>
#include <vfs/io.h>

#include <utils/kryga_log.h>
#include <utils/buffer.h>

namespace kryga
{
namespace ui
{

void
bake_editor::init(const vfs::rid& base, const vfs::rid& cache)
{
    m_config.bind(base, cache);
    m_config.load();
}

void
bake_editor::save_config()
{
    m_config.save();
}

void
bake_editor::handle()
{
}

bake_scene_info
bake_editor::collect_scene_info() const
{
    bake_scene_info info;
    auto* level = glob::glob_state().getr_model().current_level;
    info.level_loaded = level != nullptr;
    if (!level)
    {
        return info;
    }

    for (auto& [id, obj_ptr] : level->get_game_objects().get_items())
    {
        auto* go = static_cast<root::game_object*>(obj_ptr);
        for (auto* comp : go->get_renderable_components())
        {
            if (auto* mc = dynamic_cast<base::mesh_component*>(comp))
            {
                if (mc->get_layers().contribute_gi)
                {
                    ++info.static_count;
                }
            }
            if (auto* dlc = dynamic_cast<base::directional_light_component*>(comp))
            {
                if (dlc->get_selected())
                {
                    ++info.directional_count;
                }
            }
            if (dynamic_cast<base::point_light_component*>(comp) ||
                dynamic_cast<base::spot_light_component*>(comp))
            {
                ++info.local_light_count;
            }
        }
    }
    return info;
}

bool
bake_editor::submit_bake()
{
    auto* level = glob::glob_state().getr_model().current_level;
    if (!level)
    {
        return false;
    }

    auto& actions = glob::glob_state().getr_editor_system().ui.m_actions;
    if (actions.is_busy())
    {
        return false;
    }

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

    if (meshes->empty() || lights->empty())
    {
        return false;
    }

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

            glm::vec3 bb_min(std::numeric_limits<float>::max());
            glm::vec3 bb_max(std::numeric_limits<float>::lowest());
            for (auto& v : md.vertices)
            {
                auto wp = glm::vec3(md.transform * glm::vec4(v.position, 1.0f));
                bb_min = glm::min(bb_min, wp);
                bb_max = glm::max(bb_max, wp);
            }
            float max_extent =
                glm::max(bb_max.x - bb_min.x, glm::max(bb_max.y - bb_min.y, bb_max.z - bb_min.z));
            uint32_t tile_size = static_cast<uint32_t>(std::ceil(max_extent * texels_per_unit));
            tile_size = std::clamp(
                tile_size, static_cast<uint32_t>(min_tile), static_cast<uint32_t>(max_tile));
            tile_size = (tile_size + 3) & ~3u;

            constexpr uint32_t padding = 4;
            uint32_t padded_size = tile_size + padding * 2;

            if (!atlas.allocate(tile_id, padded_size, padded_size))
            {
                ALOG_WARN("bake_editor: atlas full at mesh {}, skipping remaining", i);
                break;
            }

            auto* region = atlas.get_region(tile_id);

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

        auto* cur_level = glob::glob_state().getr_model().current_level;
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

        manifest.save(baked_root / "lightmap_manifest.cfg");

        progress.set_status("Loading into renderer...");
        progress.progress.store(0.95f);

        const auto& lm_data = baker.get_lightmap_data();
        if (!lm_data.empty() && cur_level)
        {
            utils::buffer lm_buf(lm_data.size());
            std::memcpy(lm_buf.data(), lm_data.data(), lm_data.size());

            auto& loader = glob::glob_state().getr_render().loader;
            auto& renderer = glob::glob_state().getr_render().renderer;

            auto lm_tex_id = AID((cur_level->get_id().str() + "_lightmap").c_str());

            // The render-state block (lightmap registry lookup, atlas texture
            // create/update, registry bind) runs in a render-access context:
            // the bake runs on the action worker thread, and the bindless pool
            // and loader registry are render-thread-owned mid-stream. Main
            // keeps pumping frames during the bake, so the action drains
            // within ~a frame; the wait keeps lm_buf/manifest refs valid.
            render::texture_data* tex = nullptr;
            renderer.run_on_render_thread(
                [&]
                {
                    // Re-bake of an already-bound level updates the atlas in place
                    // (same bindless slot — objects keep their index); first bake
                    // creates it. The binding owns the texture.
                    const auto* binding = loader.get_lightmap(cur_level->get_id());
                    tex = binding ? binding->texture : nullptr;
                    if (tex)
                    {
                        renderer.update_texture(tex,
                                                lm_buf,
                                                result.atlas_width,
                                                result.atlas_height,
                                                VK_FORMAT_R16G16B16A16_SFLOAT,
                                                render::texture_format::rgba16f);
                    }
                    else
                    {
                        tex = renderer.create_texture(lm_tex_id,
                                                      lm_buf,
                                                      result.atlas_width,
                                                      result.atlas_height,
                                                      VK_FORMAT_R16G16B16A16_SFLOAT,
                                                      render::texture_format::rgba16f);
                    }

                    if (tex)
                    {
                        // Lightmap binding is render-owned: register the atlas
                        // index + per-object UVs in the loader's per-level
                        // registry (the model no longer caches them).
                        loader.set_lightmap(cur_level->get_id(),
                                            tex,
                                            render_translate::flatten_lightmap_manifest(manifest));
                    }
                });

            if (tex)
            {
                // NOTE: these touch MODEL state from the bake worker (level refs +
                // the dirty queue) — the remaining known editor-only violation,
                // unchanged here.
                cur_level->set_lightmap_refs(baked_root / "lightmap.bin",
                                             baked_root / "lightmap_manifest.cfg");

                for (auto& [oid, obj_ptr] : cur_level->get_game_objects().get_items())
                {
                    auto* go = static_cast<root::game_object*>(obj_ptr);
                    for (auto* comp : go->get_renderable_components())
                    {
                        if (comp->get_layers().contribute_gi)
                        {
                            glob::glob_state().getr_model().output.dirty_render.emplace_back(comp);
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
    return true;
}

}  // namespace ui
}  // namespace kryga
