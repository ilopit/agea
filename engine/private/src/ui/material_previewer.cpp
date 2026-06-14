#include "engine/private/ui/material_previewer.h"

#include <engine/kryga_engine.h>  // queue_main_action: the render->main hop

#include <global_state/global_state.h>
#include <vulkan_render/kryga_render.h>
#include <vulkan_render/render_thread.h>
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

#include <render_translator/render_translator.h>
#include <render_translator/render_convert.h>
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

// ── Sphere mesh: see material_previewer::ensure_sphere_mesh (out of the
//    anonymous namespace — it's a member that owns m_preview_sphere) ─────

// ── GPU resource helpers (RENDER stage) ─────────────────────────────

// Preview textures created here, keyed by model asset id. The loader has no
// by-id index — every owner keeps its texture_data*, and the previewer is the
// owner of these. Never released (same lifetime the old loader registry gave
// them); the pool is wiped wholesale at shutdown. Touched only from
// execute_preview (render stage).
std::unordered_map<utils::id, render::texture_data*> s_preview_textures;

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

// ── Preview job: the data crossing the main -> render -> main split ──

// One texture slot's snapshot: upload-ready pixels + binding info. Decoding
// (stbi) happens at snapshot time so the render stage only uploads.
struct preview_texture_payload
{
    uint32_t slot = 0;
    bool has_texture = false;
    utils::id texture_id;
    utils::buffer pixels;  // RGBA8, upload-ready
    uint32_t width = 0;
    uint32_t height = 0;
    bool has_sampler = false;
    uint8_t sampler_static_idx = 0;
};

struct preview_job
{
    // Request (set by render_preview before the chain starts).
    utils::id material_id;
    std::string id_str;
    uint32_t size = 128;
    std::string hash_hex;

    // Snapshot (prepare_preview, main). Self-contained: the shader blobs are
    // COPIED (make_se_ci aims se_ci's buffer pointers into the model object,
    // which main may mutate while the render stage runs) and re-pointed.
    bool snapshot_ok = false;
    bool is_session = false;
    utils::id gpu_id;  // session: instance id; class path: the material id
    render::types::material_handle stale_handle;  // session: destroy before rebuild
    render::types::material_handle reuse_handle;  // class: reuse if still live
    utils::id se_id;
    render::shader_effect_create_info se_ci;
    utils::buffer vert_copy;
    utils::buffer frag_copy;
    utils::id type_id;
    utils::dynobj gpu_data;
    std::vector<preview_texture_payload> textures;

    // Results (execute_preview, render thread).
    render::types::material_handle built_handle;
    render::offscreen_render_result result;

    // Fulfilled by complete_preview (main); the RPC I/O thread waits on it.
    std::promise<std::string> promise;
};

namespace
{

// [render stage] Texture by id from the preview registry, else create+register
// from the job's snapshot payload.
render::texture_data*
ensure_texture(const preview_texture_payload& p)
{
    auto itr = s_preview_textures.find(p.texture_id);
    if (itr != s_preview_textures.end())
    {
        return itr->second;
    }

    auto& renderer = glob::glob_state().getr_render().renderer;
    auto* td = renderer.create_texture(p.texture_id, p.pixels, p.width, p.height);
    if (td)
    {
        s_preview_textures[p.texture_id] = td;
    }
    return td;
}

// [render stage] Build the system-pool material from the job's snapshot:
// shader effect (reuse or create), preview textures, bindless index wiring.
render::material_data*
build_gpu_material(preview_job& job)
{
    auto& renderer = glob::glob_state().getr_render().renderer;

    auto* rp = renderer.get_render_pass(AID("main"));
    auto* se_data = rp->get_shader_effect(job.se_id);
    if (!se_data)
    {
        rp->create_shader_effect(job.se_id, job.se_ci, se_data);
    }
    if (!se_data || se_data->m_failed_load)
    {
        return nullptr;
    }

    std::vector<render::texture_sampler_data> samples;
    uint32_t gpu_texture_indices[KGPU_MAX_TEXTURE_SLOTS];
    uint32_t gpu_sampler_indices[KGPU_MAX_TEXTURE_SLOTS];
    for (int i = 0; i < KGPU_MAX_TEXTURE_SLOTS; ++i)
    {
        gpu_texture_indices[i] = UINT32_MAX;
        gpu_sampler_indices[i] = 0;
    }

    for (auto& p : job.textures)
    {
        if (p.has_texture)
        {
            auto* td = ensure_texture(p);
            if (td)
            {
                render::texture_sampler_data tsd;
                tsd.texture = td;
                tsd.slot = p.slot;
                samples.push_back(tsd);

                if (p.slot < KGPU_MAX_TEXTURE_SLOTS)
                {
                    gpu_texture_indices[p.slot] = td->get_bindless_index();
                }
            }
        }
        if (p.has_sampler && p.slot < KGPU_MAX_TEXTURE_SLOTS)
        {
            gpu_sampler_indices[p.slot] = p.sampler_static_idx;
        }
    }

    render_convert::set_material_texture_bindings(
        job.gpu_data, gpu_texture_indices, gpu_sampler_indices, KGPU_MAX_TEXTURE_SLOTS);

    auto* mat_data =
        renderer.create_material(job.gpu_id, job.type_id, samples, *se_data, job.gpu_data);

    if (mat_data)
    {
        for (auto& p : job.textures)
        {
            if (p.has_sampler && p.slot < KGPU_MAX_TEXTURE_SLOTS)
            {
                mat_data->set_bindless_sampler_index(p.slot, p.sampler_static_idx);
            }
        }
    }

    return mat_data;
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

// ── Sphere mesh ─────────────────────────────────────────────────────

render::mesh_data*
material_previewer::ensure_sphere_mesh()
{
    // The sphere is owned by this previewer (m_preview_sphere), freed in
    // destroy(). Cached across calls; clear_caches() (level reload) reclaims the
    // slot -> the handle goes stale -> mesh_valid is false -> rebuild.
    auto& renderer = glob::glob_state().getr_render().renderer;
    if (renderer.system_mesh_valid(m_preview_sphere))
    {
        return renderer.get_system_mesh_data(m_preview_sphere);
    }

    auto sphere = render::generate_sphere(0.85f);

    utils::buffer vert_buf(sphere.vertices.size() * sizeof(gpu::vertex_data));
    std::memcpy(vert_buf.data(), sphere.vertices.data(), vert_buf.size());

    utils::buffer idx_buf(sphere.indices.size() * sizeof(gpu::uint));
    std::memcpy(idx_buf.data(), sphere.indices.data(), idx_buf.size());

    auto* md = renderer.create_mesh(AID("preview_sphere"),
                                    vert_buf.make_view<gpu::vertex_data>(),
                                    idx_buf.make_view<gpu::uint>());
    m_preview_sphere = md ? md->render_handle() : render::types::mesh_handle{};
    return md;
}

// ── The three parts of one preview build ────────────────────────────

// [main] Resolve the model material (edit session or class cache) and snapshot
// everything the render stage needs. After this returns, main keeps ticking —
// nothing below may be touched by the later stages.
void
material_previewer::prepare_preview(preview_job& job)
{
    root::material* mat_model = nullptr;

    auto sit = m_sessions.find(job.id_str);
    if (sit != m_sessions.end() && sit->second.instance)
    {
        auto& session = sit->second;
        mat_model = &session.instance->asr<root::material>();

        // Always rebuild a session preview — properties may be modified via
        // generic property.set without going through invalidate(). Claim the
        // old handle for destruction; clearing it on the session means a
        // discard racing this job destroys nothing (no double-free), and
        // complete() re-assigns the rebuilt handle if the session survives.
        job.is_session = true;
        job.gpu_id = session.instance_id;
        job.stale_handle = session.gpu_handle;
        session.gpu_handle = {};
    }
    else
    {
        // NOTE: we deliberately do NOT reuse the scene's live CONTENT material.
        // Content material storage is render-thread-owned; we clone from the
        // MODEL into our own system-pool material — for materials that's cheap
        // (bindless, no GPU upload) and the result is identical.
        auto* obj = glob::glob_state().getr_model().caches.materials.get_item(job.material_id);
        if (!obj)
        {
            return;  // snapshot_ok stays false -> execute no-ops -> empty result
        }
        mat_model = &obj->asr<root::material>();
        job.gpu_id = job.material_id;

        // A preview-only GPU material built earlier; liveness is render-side
        // state, so the handle is checked (and reused) at execute time.
        auto cit = m_gpu_materials.find(job.id_str);
        if (cit != m_gpu_materials.end())
        {
            job.reuse_handle = cit->second;
        }
    }

    auto* se_model = mat_model->get_shader_effect();
    if (!se_model)
    {
        return;
    }

    job.se_id = se_model->get_id();
    job.se_ci = render_convert::make_se_ci(*se_model);
    job.se_ci.spec_constants = render_convert::collect_spec_constants(*se_model);
    // make_se_ci aims the blob pointers into the MODEL object; the job outlives
    // this stage, so own copies and re-point.
    job.vert_copy = *job.se_ci.vert_buffer;
    job.frag_copy = *job.se_ci.frag_buffer;
    job.se_ci.vert_buffer = &job.vert_copy;
    job.se_ci.frag_buffer = &job.frag_copy;

    job.type_id = mat_model->get_type_id();
    auto collected = render_convert::collect_gpu_data(*mat_model);
    job.gpu_data = std::move(collected.gpu_data);

    for (uint32_t i = 0; i < collected.texture_slot_count; ++i)
    {
        auto& ts = *static_cast<const root::texture_slot*>(collected.texture_slots[i].data);
        preview_texture_payload p;
        p.slot = collected.texture_slots[i].slot;
        if (ts.txt)
        {
            p.texture_id = ts.txt->get_id();
            auto& bc = ts.txt->get_mutable_base_color();
            p.width = ts.txt->get_width();
            p.height = ts.txt->get_height();
            if (asset_importer::texture_importer::is_kryga_texture(bc.get_file()))
            {
                p.pixels = bc;  // cooked == upload-ready
                p.has_texture = true;
            }
            else
            {
                p.has_texture = asset_importer::texture_importer::extract_texture_from_buffer(
                    bc, p.pixels, p.width, p.height);
            }
        }
        if (ts.smp)
        {
            p.has_sampler = true;
            p.sampler_static_idx = render_convert::map_sampler_to_static_index(*ts.smp);
        }
        if (p.has_texture || p.has_sampler)
        {
            job.textures.push_back(std::move(p));
        }
    }

    job.snapshot_ok = true;
}

// [render thread] Destroy the stale session material, build (or reuse) the
// preview material, draw the offscreen sphere. Touches ONLY the snapshot and
// render-stage state (system pools, s_preview_textures, m_renderer,
// m_preview_sphere).
void
material_previewer::execute_preview(preview_job& job)
{
    auto& renderer = glob::glob_state().getr_render().renderer;

    if (job.stale_handle)
    {
        renderer.destroy_system_material_data(job.stale_handle);
    }

    render::material_data* mat = nullptr;
    if (job.reuse_handle && renderer.system_material_valid(job.reuse_handle))
    {
        mat = renderer.get_system_material_data(job.reuse_handle);
        job.built_handle = job.reuse_handle;
    }
    else if (job.snapshot_ok)
    {
        mat = build_gpu_material(job);
        if (mat)
        {
            job.built_handle = mat->render_handle();
        }
    }
    if (!mat)
    {
        return;  // result stays empty -> complete() reports failure
    }

    auto* se = mat->get_shader_effect();
    if (!se || se->m_pipeline == VK_NULL_HANDLE || se->m_system)
    {
        return;
    }

    auto* main_pass = renderer.get_render_pass(AID("main"));
    if (se->get_owner_render_pass() != main_pass)
    {
        return;
    }

    auto& device = glob::glob_state().getr_render().device;
    m_renderer.init(device, main_pass, job.size);

    auto* sphere = ensure_sphere_mesh();
    if (!sphere)
    {
        return;
    }

    renderer.flush_pending_texture_updates();

    auto req = make_preview_request(mat, sphere, renderer.get_bindless_set());
    job.result = m_renderer.render(device, req);
}

// [main] Re-attach the built handle to the session/registry (or schedule its
// destruction if the session vanished mid-flight), encode + cache the image,
// fulfil the waiting promise.
void
material_previewer::complete_preview(preview_job& job)
{
    auto& renderer = glob::glob_state().getr_render().renderer;

    if (job.is_session)
    {
        auto sit = m_sessions.find(job.id_str);
        if (sit != m_sessions.end() && sit->second.instance)
        {
            sit->second.gpu_handle = job.built_handle;
        }
        else if (job.built_handle)
        {
            // Session discarded while the job was in flight: the rebuilt
            // material has no owner. Fire-and-forget free (by-value handle).
            renderer.post_render_action([&renderer, h = job.built_handle]
                                        { renderer.destroy_system_material_data(h); });
        }
    }
    else if (job.built_handle)
    {
        m_gpu_materials[job.id_str] = job.built_handle;
    }

    std::string b64;
    if (!job.result.pixels.empty())
    {
        std::vector<uint8_t> png_buf;
        stbi_write_png_to_func(png_write_callback,
                               &png_buf,
                               int(job.result.width),
                               int(job.result.height),
                               4,
                               job.result.pixels.data(),
                               int(job.result.width * 4));
        if (!png_buf.empty())
        {
            b64 = base64_encode(png_buf.data(), png_buf.size());
            m_memory_cache[job.id_str] = {b64, job.hash_hex};
            save_to_fs(job.id_str, job.hash_hex, png_buf);
        }
    }

    m_inflight.erase(job.id_str);
    job.promise.set_value(std::move(b64));
}

// ── Public API ─────────────────────────────────────────────────────

std::shared_future<std::string>
material_previewer::render_preview(const utils::id& material_id, uint32_t size)
{
    auto ready = [](std::string v)
    {
        std::promise<std::string> p;
        p.set_value(std::move(v));
        return p.get_future().share();
    };

    size = std::clamp(size, 32u, 512u);
    auto id_str = material_id.str();

    load_registry();

    // Hash against the current model state; cache hits resolve immediately.
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
            return ready({});
        }
        mat_model = &obj->asr<root::material>();
    }

    auto hash_hex = compute_content_hash(*mat_model);

    auto mc_it = m_memory_cache.find(id_str);
    if (mc_it != m_memory_cache.end() && mc_it->second.content_hash_hex == hash_hex)
    {
        return ready(mc_it->second.base64_png);
    }

    auto b64 = try_load_from_fs(id_str, hash_hex);
    if (!b64.empty())
    {
        m_memory_cache[id_str] = {b64, hash_hex};
        return ready(b64);
    }

    // One job per material id at a time: a second request joins the first's
    // future instead of double-destroying the session's old material.
    auto fit = m_inflight.find(id_str);
    if (fit != m_inflight.end())
    {
        return fit->second;
    }

    auto job = std::make_shared<preview_job>();
    job->material_id = material_id;
    job->id_str = id_str;
    job->size = size;
    job->hash_hex = hash_hex;
    auto fut = job->promise.get_future().share();

    prepare_preview(*job);  // [main] model -> snapshot; no model access after

    auto& renderer = glob::glob_state().getr_render().renderer;
    if (render::on_render_thread())
    {
        // Single-threaded context (headless / teardown): no queues to cross,
        // no main-action drain coming — run the remaining parts inline.
        execute_preview(*job);
        complete_preview(*job);
        return fut;
    }

    m_inflight[id_str] = fut;
    renderer.post_render_action(
        [this, job]
        {
            execute_preview(*job);
            glob::glob_state().getr_engine().queue_main_action([this, job]
                                                               { complete_preview(*job); });
        });
    return fut;
}

void
material_previewer::clear_cache()
{
    m_memory_cache.clear();
}

void
material_previewer::invalidate(const utils::id& material_id)
{
    m_memory_cache.erase(material_id.str());
}

void
material_previewer::destroy()
{
    discard_all_edits();
    auto& renderer = glob::glob_state().getr_render().renderer;
    // Teardown path: the render loop is gone and main holds render access, so
    // this runs inline (mid-stream destroy() doesn't exist).
    renderer.post_render_action([&renderer, h = m_preview_sphere]
                                { renderer.destroy_system_mesh_data(h); });
    m_preview_sphere = {};
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

    {
        KRG_CONSTRUCTION_TRANSACTION(*class_obj);

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
    }

    auto& class_mat = class_obj->asr<root::material>();

    auto* pkg = class_obj->get_package();
    KRG_check(pkg, "class material has no package");

    core::object_constructor ctor(&pkg->get_load_context());
    ctor.save_obj(*class_obj);

    auto& renderer = glob::glob_state().getr_render().renderer;
    // System-pool free is render-thread-owned mid-stream; fire-and-forget by
    // value-captured handle (inline at teardown, queued mid-stream).
    renderer.post_render_action([&renderer, h = session.gpu_handle]
                                { renderer.destroy_system_material_data(h); });
    session.gpu_handle = {};

    auto& model = glob::glob_state().getr_model();
    core::object_constructor inst_ctor(&pkg->get_load_context(),
                                       core::object_load_type::instance_obj);

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
                auto& src_ts = *reinterpret_cast<const root::texture_slot*>(src_blob + p->offset);
                auto& dst_ts = *reinterpret_cast<root::texture_slot*>(dst_blob + p->offset);

                dst_ts.slot = src_ts.slot;

                if (src_ts.txt)
                {
                    auto* inst_txt = model.caches.textures.get_item(src_ts.txt->get_id());
                    if (!inst_txt)
                    {
                        auto r = inst_ctor.instantiate_obj(*src_ts.txt, src_ts.txt->get_id());
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
                    auto* inst_smp = model.caches.samplers.get_item(src_ts.smp->get_id());
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
    auto& renderer = glob::glob_state().getr_render().renderer;
    renderer.post_render_action([&renderer, h = session.gpu_handle]
                                { renderer.destroy_system_material_data(h); });
    session.gpu_handle = {};

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
    auto& renderer = glob::glob_state().getr_render().renderer;

    for (auto& [id_str, session] : m_sessions)
    {
        renderer.post_render_action([&renderer, h = session.gpu_handle]
                                    { renderer.destroy_system_material_data(h); });

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
