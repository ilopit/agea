#include "engine/private/ui/material_previewer.h"

#include <global_state/global_state.h>
#include <vulkan_render/kryga_render.h>
#include <vulkan_render/vulkan_render_loader.h>
#include <vulkan_render/vulkan_render_device.h>
#include <vulkan_render/render_system.h>
#include <vulkan_render/types/vulkan_material_data.h>
#include <vulkan_render/types/vulkan_mesh_data.h>
#include <vulkan_render/types/vulkan_shader_effect_data.h>
#include <vulkan_render/types/vulkan_render_pass.h>

#include <core/caches/caches_map.h>
#include <core/model_system.h>
#include <core/object_constructor.h>
#include <core/object_load_context.h>
#include <core/package.h>
#include <core/reflection/property.h>
#include <core/reflection/property_utils.h>
#include <core/reflection/reflection_type.h>

#include <render_bridge/render_bridge.h>
#include <render/utils/mesh_primitives.h>

#include <vfs/vfs.h>
#include <vfs/rid.h>

#include <packages/root/model/assets/material.h>
#include <packages/root/model/assets/texture_slot.h>
#include <packages/root/model/assets/shader_effect.h>
#include <packages/root/model/assets/texture.h>
#include <packages/root/model/assets/sampler.h>

#include <assets_importer/texture_importer.h>

#include <utils/base64.h>
#include <utils/buffer.h>
#include <utils/fnv_hash.h>

#include <gpu_types/gpu_generic_constants.h>
#include <gpu_types/gpu_vertex_types.h>

#include <json/json.h>
#include <stb_unofficial/stb.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace kryga::ui
{

using namespace kryga::render;

namespace
{

void
png_write_callback(void* ctx, void* data, int size)
{
    auto* buf = static_cast<std::vector<uint8_t>*>(ctx);
    auto* bytes = static_cast<uint8_t*>(data);
    buf->insert(buf->end(), bytes, bytes + size);
}

std::string
compute_content_hash(root::material& mat)
{
    fnv_hasher ch;

    ch.feed_str(mat.get_type_id().str());

    auto* se = mat.get_shader_effect();
    if (se)
    {
        ch.feed_str(se->get_id().str());
    }

    auto* rt = mat.get_reflection();
    if (rt)
    {
        for (auto& p : rt->m_serialization_properties)
        {
            if (p->type.is_collection || p->type.is_ptr)
            {
                continue;
            }
            Json::Value val;
            kryga::reflection::property_context__json_get ctx{p.get(), &mat, &val};
            if (p->json_get(ctx) == result_code::ok)
            {
                Json::StreamWriterBuilder b;
                b["indentation"] = "";
                auto s = Json::writeString(b, val);
                ch.feed_str(s);
            }
        }
    }

    return ch.hex();
}

// ── Sphere mesh ─────────────────────────────────────────────────────

render::mesh_data*
ensure_sphere_mesh()
{
    auto& loader = glob::glob_state().getr_render().loader;
    auto* existing = loader.get_mesh_data(AID("preview_sphere"));
    if (existing)
    {
        return existing;
    }

    auto sphere = render::generate_sphere(0.85f);

    utils::buffer vert_buf(sphere.vertices.size() * sizeof(gpu::vertex_data));
    std::memcpy(vert_buf.data(), sphere.vertices.data(), vert_buf.size());

    utils::buffer idx_buf(sphere.indices.size() * sizeof(gpu::uint));
    std::memcpy(idx_buf.data(), sphere.indices.data(), idx_buf.size());

    return loader.create_mesh(AID("preview_sphere"),
                              vert_buf.make_view<gpu::vertex_data>(),
                              idx_buf.make_view<gpu::uint>());
}

// ── GPU resource helpers ────────────────────────────────────────────

render::shader_effect_data*
ensure_shader_effect(root::shader_effect& se_model)
{
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto* rp = renderer.get_render_pass(AID("main"));
    auto* existing = rp->get_shader_effect(se_model.get_id());
    if (existing)
    {
        return existing;
    }

    auto se_ci = render_bridge::make_se_ci(se_model);
    se_ci.spec_constants = render_bridge::collect_spec_constants(se_model);

    render::shader_effect_data* se_data = nullptr;
    rp->create_shader_effect(se_model.get_id(), se_ci, se_data);
    return se_data;
}

render::texture_data*
ensure_texture(root::texture& txt_model)
{
    auto& loader = glob::glob_state().getr_render().loader;
    auto* existing = loader.get_texture_data(txt_model.get_id());
    if (existing)
    {
        return existing;
    }

    auto& bc = txt_model.get_mutable_base_color();
    auto w = txt_model.get_width();
    auto h = txt_model.get_height();

    if (render_bridge::is_kryga_texture(bc.get_file()))
    {
        return loader.create_texture(txt_model.get_id(), bc, w, h);
    }

    utils::buffer pixels;
    if (!asset_importer::texture_importer::extract_texture_from_buffer(bc, pixels, w, h))
    {
        return nullptr;
    }
    return loader.create_texture(txt_model.get_id(), pixels, w, h);
}

render::material_data*
create_gpu_material_from_model(const utils::id& gpu_id, root::material& mat_model)
{
    auto& loader = glob::glob_state().getr_render().loader;
    auto* se_model = mat_model.get_shader_effect();
    if (!se_model)
    {
        return nullptr;
    }

    auto* se_data = ensure_shader_effect(*se_model);
    if (!se_data || se_data->m_failed_load)
    {
        return nullptr;
    }

    auto collected = glob::glob_state().getr_render_bridge().collect_gpu_data(mat_model);

    std::vector<render::texture_sampler_data> samples;
    uint32_t gpu_texture_indices[KGPU_MAX_TEXTURE_SLOTS];
    uint32_t gpu_sampler_indices[KGPU_MAX_TEXTURE_SLOTS];
    for (int i = 0; i < KGPU_MAX_TEXTURE_SLOTS; ++i)
    {
        gpu_texture_indices[i] = UINT32_MAX;
        gpu_sampler_indices[i] = 0;
    }

    for (uint32_t i = 0; i < collected.texture_slot_count; ++i)
    {
        auto slot = collected.texture_slots[i].slot;
        auto& ts = *static_cast<const root::texture_slot*>(collected.texture_slots[i].data);
        if (ts.txt)
        {
            auto* td = ensure_texture(*ts.txt);
            if (td)
            {
                render::texture_sampler_data tsd;
                tsd.texture = td;
                tsd.slot = slot;
                samples.push_back(tsd);

                if (slot < KGPU_MAX_TEXTURE_SLOTS)
                {
                    gpu_texture_indices[slot] = td->get_bindless_index();
                }
            }
        }
        if (ts.smp && slot < KGPU_MAX_TEXTURE_SLOTS)
        {
            gpu_sampler_indices[slot] = render_bridge::map_sampler_to_static_index(*ts.smp);
        }
    }

    render_bridge::set_material_texture_bindings(
        collected.gpu_data, gpu_texture_indices, gpu_sampler_indices, KGPU_MAX_TEXTURE_SLOTS);

    auto* mat_data = loader.create_material(
        gpu_id, mat_model.get_type_id(), samples, *se_data, collected.gpu_data);

    if (mat_data)
    {
        for (uint32_t i = 0; i < collected.texture_slot_count; ++i)
        {
            auto slot = collected.texture_slots[i].slot;
            auto& ts = *static_cast<const root::texture_slot*>(collected.texture_slots[i].data);
            if (ts.smp && slot < KGPU_MAX_TEXTURE_SLOTS)
            {
                mat_data->set_bindless_sampler_index(
                    slot, render_bridge::map_sampler_to_static_index(*ts.smp));
            }
        }
    }

    return mat_data;
}

offscreen_draw_request
make_preview_request(render::material_data* mat,
                     render::mesh_data* mesh,
                     VkDescriptorSet bindless_set)
{
    auto* se = mat->get_shader_effect();

    offscreen_draw_request req;
    req.mesh = mesh;
    req.shader_effect = se;
    req.bindless_set = bindless_set;

    req.camera.projection = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 10.0f);
    req.camera.projection[1][1] *= -1.0f;
    req.camera.view =
        glm::lookAt(glm::vec3(0.0f, 0.0f, 2.5f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    req.camera.inv_projection = glm::inverse(req.camera.projection);
    req.camera.position = glm::vec3(0.0f, 0.0f, 2.5f);

    req.object.model = glm::mat4(1.0f);
    req.object.normal = glm::mat4(1.0f);
    req.object.lightmap_scale = glm::vec2(1.0f);
    req.object.lightmap_offset = glm::vec2(0.0f);
    req.object.lightmap_texture_index = UINT32_MAX;
    req.object.probe_index = UINT32_MAX;

    req.directional_light.direction[0] = -0.5f;
    req.directional_light.direction[1] = -0.7f;
    req.directional_light.direction[2] = -0.5f;
    float len = std::sqrt(0.5f * 0.5f + 0.7f * 0.7f + 0.5f * 0.5f);
    req.directional_light.direction[0] /= len;
    req.directional_light.direction[1] /= len;
    req.directional_light.direction[2] /= len;
    req.directional_light.ambient[0] = 0.15f;
    req.directional_light.ambient[1] = 0.15f;
    req.directional_light.ambient[2] = 0.15f;
    req.directional_light.diffuse[0] = 0.9f;
    req.directional_light.diffuse[1] = 0.9f;
    req.directional_light.diffuse[2] = 0.9f;
    req.directional_light.specular[0] = 1.0f;
    req.directional_light.specular[1] = 1.0f;
    req.directional_light.specular[2] = 1.0f;

    const auto& gpu_data = mat->get_gpu_data();
    if (!gpu_data.empty())
    {
        req.material_gpu_data = gpu_data.data();
        req.material_gpu_data_size = gpu_data.size();
    }

    for (uint32_t i = 0; i < KGPU_MAX_TEXTURE_SLOTS; ++i)
    {
        req.texture_indices[i] = mat->get_bindless_texture_index(i);
        req.sampler_indices[i] = mat->get_bindless_sampler_index(i);
    }

    return req;
}

utils::id
make_edit_id(const utils::id& class_id)
{
    return AID("__edit_" + class_id.str());
}

}  // namespace

// ── FS cache registry ──────────────────────────────────────────────

void
material_previewer::load_registry()
{
    if (m_fs_registry_loaded)
    {
        return;
    }
    m_fs_registry_loaded = true;

    auto& vfs = glob::glob_state().getr_vfs();
    std::string json_str;
    if (!vfs.read_string(vfs::rid("tmp", "preview_cache/registry.json"), json_str))
    {
        return;
    }

    Json::CharReaderBuilder b;
    std::string errs;
    std::istringstream is(json_str);
    Json::Value root;
    if (!Json::parseFromStream(b, is, &root, &errs))
    {
        return;
    }

    if (!root.isObject() || !root.isMember("entries"))
    {
        return;
    }

    auto& entries = root["entries"];
    for (auto it = entries.begin(); it != entries.end(); ++it)
    {
        auto& val = *it;
        fs_cache_entry entry;
        entry.hash_hex = val["hash"].asString();
        entry.filename = val["file"].asString();
        m_fs_registry[it.key().asString()] = entry;
    }
}

void
material_previewer::save_registry()
{
    if (!m_fs_registry_dirty)
    {
        return;
    }

    auto& vfs = glob::glob_state().getr_vfs();
    vfs.create_directories(vfs::rid("tmp", "preview_cache"));

    Json::Value root(Json::objectValue);
    root["version"] = 1;
    Json::Value entries(Json::objectValue);
    for (auto& [id_str, entry] : m_fs_registry)
    {
        Json::Value e(Json::objectValue);
        e["hash"] = entry.hash_hex;
        e["file"] = entry.filename;
        entries[id_str] = e;
    }
    root["entries"] = entries;

    Json::StreamWriterBuilder b;
    b["indentation"] = "  ";
    auto json_str = Json::writeString(b, root);
    vfs.write_string(vfs::rid("tmp", "preview_cache/registry.json"), json_str);
    m_fs_registry_dirty = false;
}

std::string
material_previewer::try_load_from_fs(const std::string& id_str, const std::string& hash_hex)
{
    auto it = m_fs_registry.find(id_str);
    if (it == m_fs_registry.end() || it->second.hash_hex != hash_hex)
    {
        return {};
    }

    auto& vfs = glob::glob_state().getr_vfs();
    std::vector<uint8_t> png_bytes;
    if (!vfs.read_bytes(vfs::rid("tmp", "preview_cache/" + it->second.filename), png_bytes) ||
        png_bytes.empty())
    {
        m_fs_registry.erase(it);
        m_fs_registry_dirty = true;
        return {};
    }

    return base64_encode(png_bytes.data(), png_bytes.size());
}

void
material_previewer::save_to_fs(const std::string& id_str,
                               const std::string& hash_hex,
                               const std::vector<uint8_t>& png_data)
{
    auto& vfs = glob::glob_state().getr_vfs();
    vfs.create_directories(vfs::rid("tmp", "preview_cache"));

    auto it = m_fs_registry.find(id_str);
    if (it != m_fs_registry.end() && it->second.hash_hex != hash_hex)
    {
        vfs.remove(vfs::rid("tmp", "preview_cache/" + it->second.filename));
    }

    std::string filename = hash_hex + ".png";
    vfs.write_bytes(vfs::rid("tmp", "preview_cache/" + filename),
                    std::span<const uint8_t>(png_data.data(), png_data.size()));

    m_fs_registry[id_str] = {hash_hex, filename};
    m_fs_registry_dirty = true;
    save_registry();
}

// ── GPU material management ────────────────────────────────────────

render::material_data*
material_previewer::ensure_gpu_material(const utils::id& material_id)
{
    auto& loader = glob::glob_state().getr_render().loader;

    auto sit = m_sessions.find(material_id.str());
    if (sit != m_sessions.end() && sit->second.instance)
    {
        auto& session = sit->second;
        auto& inst_mat = session.instance->asr<root::material>();

        if (session.dirty)
        {
            loader.destroy_material_data(session.instance_id);
            auto* mat = create_gpu_material_from_model(session.instance_id, inst_mat);
            session.dirty = false;
            return mat;
        }

        auto* existing = loader.get_material_data(session.instance_id);
        if (existing)
        {
            return existing;
        }

        auto* mat = create_gpu_material_from_model(session.instance_id, inst_mat);
        session.dirty = false;
        return mat;
    }

    auto* existing = loader.get_material_data(material_id);
    if (existing)
    {
        return existing;
    }

    auto* obj = glob::glob_state().getr_model().caches.materials.get_item(material_id);
    if (!obj)
    {
        return nullptr;
    }
    return create_gpu_material_from_model(material_id, obj->asr<root::material>());
}

// ── Public API ─────────────────────────────────────────────────────

std::string
material_previewer::render_preview(const utils::id& material_id, uint32_t size)
{
    size = std::clamp(size, 32u, 512u);
    auto id_str = material_id.str();

    load_registry();

    root::material* mat_model = nullptr;
    auto sit = m_sessions.find(id_str);
    if (sit != m_sessions.end() && sit->second.instance)
    {
        mat_model = &sit->second.instance->asr<root::material>();
    }
    else
    {
        auto* obj = glob::glob_state().getr_model().caches.materials.get_item(material_id);
        if (!obj)
        {
            return {};
        }
        mat_model = &obj->asr<root::material>();
    }

    auto hash_hex = compute_content_hash(*mat_model);

    auto mc_it = m_memory_cache.find(id_str);
    if (mc_it != m_memory_cache.end() && mc_it->second.content_hash_hex == hash_hex)
    {
        return mc_it->second.base64_png;
    }

    auto b64 = try_load_from_fs(id_str, hash_hex);
    if (!b64.empty())
    {
        m_memory_cache[id_str] = {b64, hash_hex};
        return b64;
    }

    auto* mat = ensure_gpu_material(material_id);
    if (!mat)
    {
        return {};
    }

    auto* se = mat->get_shader_effect();
    if (!se || se->m_pipeline == VK_NULL_HANDLE || se->m_system)
    {
        return {};
    }

    auto& device = glob::glob_state().getr_render().device;
    auto& renderer = glob::glob_state().getr_render().renderer;

    auto* main_pass = renderer.get_render_pass(AID("main"));
    if (se->get_owner_render_pass() != main_pass)
    {
        return {};
    }

    m_renderer.init(device, main_pass, size);

    auto* sphere = ensure_sphere_mesh();
    if (!sphere)
    {
        return {};
    }

    renderer.flush_pending_texture_updates();

    auto req = make_preview_request(mat, sphere, renderer.get_bindless_set());
    auto result = m_renderer.render(device, req);
    if (result.pixels.empty())
    {
        return {};
    }

    std::vector<uint8_t> png_buf;
    stbi_write_png_to_func(png_write_callback,
                           &png_buf,
                           int(result.width),
                           int(result.height),
                           4,
                           result.pixels.data(),
                           int(result.width * 4));

    if (png_buf.empty())
    {
        return {};
    }

    b64 = base64_encode(png_buf.data(), png_buf.size());

    m_memory_cache[id_str] = {b64, hash_hex};
    save_to_fs(id_str, hash_hex, png_buf);

    return b64;
}

void
material_previewer::clear_cache()
{
    m_memory_cache.clear();
}

void
material_previewer::invalidate(const utils::id& material_id)
{
    auto id_str = material_id.str();

    auto sit = m_sessions.find(id_str);
    if (sit != m_sessions.end())
    {
        sit->second.dirty = true;
    }

    m_memory_cache.erase(id_str);
}

void
material_previewer::destroy()
{
    discard_all_edits();
    m_memory_cache.clear();
    save_registry();
    m_renderer.destroy(glob::glob_state().getr_render().device.vk_device());
}

root::smart_object*
material_previewer::begin_edit(const utils::id& material_id)
{
    auto id_str = material_id.str();

    auto sit = m_sessions.find(id_str);
    if (sit != m_sessions.end())
    {
        return sit->second.instance;
    }

    auto* class_obj = glob::glob_state().getr_model().caches.materials.get_item(material_id);
    KRG_check(class_obj, "material not found in class cache");

    auto* pkg = class_obj->get_package();
    KRG_check(pkg, "material has no package");

    auto& olc = pkg->get_load_context();
    core::object_constructor ctor(&olc, core::object_load_type::instance_obj);

    auto edit_id = make_edit_id(material_id);
    auto result = ctor.instantiate_obj(*class_obj, edit_id);
    KRG_check(result.has_value(), "failed to instantiate material for editing");

    edit_session session;
    session.class_id = material_id;
    session.instance_id = edit_id;
    session.instance = result.value();
    session.dirty = true;

    m_sessions[id_str] = session;
    return session.instance;
}

root::smart_object*
material_previewer::get_editing(const utils::id& material_id)
{
    auto sit = m_sessions.find(material_id.str());
    if (sit != m_sessions.end())
    {
        return sit->second.instance;
    }
    return nullptr;
}

bool
material_previewer::save_edit(const utils::id& material_id)
{
    auto id_str = material_id.str();
    auto sit = m_sessions.find(id_str);
    if (sit == m_sessions.end())
    {
        return false;
    }

    auto& session = sit->second;
    auto* instance = session.instance;

    auto* class_obj = glob::glob_state().getr_model().caches.materials.get_item(material_id);
    KRG_check(class_obj, "class material disappeared during edit");

    auto* rt = instance->get_reflection();
    KRG_check(rt, "edited instance has no reflection");

    for (auto& cat_pair : rt->m_editor_properties)
    {
        if (cat_pair.first == "Meta")
        {
            continue;
        }
        for (auto& p : cat_pair.second)
        {
            if (!p->serializable || !p->rtype || !p->rtype->json_load)
            {
                continue;
            }
            if (p->type.is_collection || p->type.is_ptr)
            {
                continue;
            }

            Json::Value val;
            kryga::reflection::property_context__json_get get_ctx{p.get(), instance, &val};
            if (p->json_get(get_ctx) != result_code::ok)
            {
                continue;
            }

            kryga::reflection::property_context__json_set set_ctx{p.get(), class_obj, &val};
            p->json_set(set_ctx);
        }
    }

    auto& class_mat = class_obj->asr<root::material>();

    auto& vfs = glob::glob_state().getr_vfs();
    auto* pkg = class_obj->get_package();
    KRG_check(pkg, "class material has no package");

    auto found = vfs.find_object(pkg->get_vfs_root(), material_id.str());
    if (found)
    {
        auto phys = vfs.real_path(*found);
        if (phys)
        {
            core::object_constructor::object_save(*class_obj, utils::path(phys->string()));
        }
    }

    auto& loader = glob::glob_state().getr_render().loader;
    loader.destroy_material_data(session.instance_id);

    auto& model = glob::glob_state().getr_model();
    auto& olc = pkg->get_load_context();
    core::object_constructor ctor(&olc, core::object_load_type::instance_obj);

    model.caches.materials.call_on_items(
        [&](root::material* inst_mat)
        {
            if (inst_mat->get_class_obj() != class_obj)
            {
                return true;
            }
            if (!inst_mat->get_flags().instance_obj)
            {
                return true;
            }

            auto src_blob = class_obj->as_blob();
            auto dst_blob = inst_mat->as_blob();

            for (auto& p : rt->m_serialization_properties)
            {
                if (p->gpu_texture_slot < 0)
                {
                    continue;
                }
                auto& src_ts =
                    *reinterpret_cast<const root::texture_slot*>(src_blob + p->offset);
                auto& dst_ts = *reinterpret_cast<root::texture_slot*>(dst_blob + p->offset);

                dst_ts.slot = src_ts.slot;

                if (src_ts.txt)
                {
                    auto* inst_txt =
                        model.caches.textures.get_item(src_ts.txt->get_id());
                    if (!inst_txt)
                    {
                        auto r = ctor.instantiate_obj(*src_ts.txt, src_ts.txt->get_id());
                        if (r)
                        {
                            inst_txt = r.value()->as<root::texture>();
                        }
                    }
                    dst_ts.txt = inst_txt;
                }
                else
                {
                    dst_ts.txt = nullptr;
                }

                if (src_ts.smp)
                {
                    auto* inst_smp =
                        model.caches.samplers.get_item(src_ts.smp->get_id());
                    if (inst_smp)
                    {
                        dst_ts.smp = inst_smp;
                    }
                }
                else
                {
                    dst_ts.smp = nullptr;
                }
            }

            inst_mat->mark_render_dirty();
            return true;
        });

    pkg->get_load_context().remove_obj(*instance);

    m_memory_cache.erase(id_str);
    m_sessions.erase(sit);

    return true;
}

void
material_previewer::discard_edit(const utils::id& material_id)
{
    auto id_str = material_id.str();
    auto sit = m_sessions.find(id_str);
    if (sit == m_sessions.end())
    {
        return;
    }

    auto& session = sit->second;
    auto& loader = glob::glob_state().getr_render().loader;
    loader.destroy_material_data(session.instance_id);

    auto* class_obj = glob::glob_state().getr_model().caches.materials.get_item(material_id);
    if (class_obj)
    {
        auto* pkg = class_obj->get_package();
        if (pkg)
        {
            pkg->get_load_context().remove_obj(*session.instance);
        }
    }

    m_memory_cache.erase(id_str);
    m_sessions.erase(sit);
}

void
material_previewer::discard_all_edits()
{
    auto& loader = glob::glob_state().getr_render().loader;

    for (auto& [id_str, session] : m_sessions)
    {
        loader.destroy_material_data(session.instance_id);

        auto* class_obj =
            glob::glob_state().getr_model().caches.materials.get_item(session.class_id);
        if (class_obj)
        {
            auto* pkg = class_obj->get_package();
            if (pkg)
            {
                pkg->get_load_context().remove_obj(*session.instance);
            }
        }
    }

    m_sessions.clear();
}

}  // namespace kryga::ui
