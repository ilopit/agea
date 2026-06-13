#pragma once

#include <vulkan_render/utils/offscreen_renderer.h>
#include <vulkan_render/types/vulkan_render_types_fwds.h>

#include <utils/id.h>
#include <render_types/render_handle.h>

#include <cstdint>
#include <future>
#include <string>
#include <unordered_map>

namespace kryga
{
namespace root
{
class smart_object;
}

namespace ui
{

// Everything one preview build carries across the main -> render -> main
// split: the model snapshot (filled by prepare_preview), the GPU results
// (filled by execute_preview), and the promise the waiting RPC thread is
// parked on (fulfilled by complete_preview). Defined in the .cpp.
struct preview_job;

struct fs_cache_entry
{
    std::string hash_hex;
    std::string filename;
};

struct memory_cache_entry
{
    std::string base64_png;
    std::string content_hash_hex;
};

struct edit_session
{
    utils::id class_id;
    utils::id instance_id;
    root::smart_object* instance = nullptr;
    render::types::material_handle gpu_handle{};  // GPU material built for this session
};

class material_previewer
{
public:
    // Async: cache hits resolve the future immediately; a miss splits the
    // build across the threads that own each part -- prepare_preview (main:
    // model snapshot), execute_preview (render thread: GPU build + offscreen
    // draw), complete_preview (main: encode + caches + fulfil) -- chained
    // through the render-/main-action queues. Main never blocks; the caller
    // (an RPC I/O thread) waits on the future, which resolves ~two frames
    // later. An empty string means "not found / build failed".
    std::shared_future<std::string>
    render_preview(const utils::id& material_id, uint32_t size = 128);

    void
    clear_cache();

    void
    invalidate(const utils::id& material_id);

    void
    destroy();

    root::smart_object*
    begin_edit(const utils::id& material_id);

    root::smart_object*
    get_editing(const utils::id& material_id);

    bool
    save_edit(const utils::id& material_id);

    void
    discard_edit(const utils::id& material_id);

    void
    discard_all_edits();

private:
    void
    load_registry();

    void
    save_registry();

    std::string
    try_load_from_fs(const std::string& id_str, const std::string& hash_hex);

    void
    save_to_fs(const std::string& id_str,
               const std::string& hash_hex,
               const std::vector<uint8_t>& png_data);

    render::mesh_data*
    ensure_sphere_mesh();

    // The three parts of one preview build. Each runs on the thread that owns
    // the state it touches; render_preview chains them through the action
    // queues (or runs them inline when this thread holds render access --
    // headless / teardown).
    // [main] model -> self-contained snapshot; no model access past this.
    void
    prepare_preview(preview_job& job);
    // [render thread] system material/texture/mesh build + offscreen draw.
    void
    execute_preview(preview_job& job);
    // [main] handle bookkeeping, PNG/base64, caches, fulfil the promise.
    void
    complete_preview(preview_job& job);

    render::offscreen_renderer m_renderer;
    render::types::mesh_handle m_preview_sphere{};  // owned preview sphere; freed in destroy()
    std::unordered_map<std::string, edit_session> m_sessions;
    std::unordered_map<std::string, memory_cache_entry> m_memory_cache;
    // Preview-only GPU materials this previewer created (id_str -> handle). Our
    // own id->handle index, kept outside the render loader (which is handle-only).
    std::unordered_map<std::string, render::types::material_handle> m_gpu_materials;
    // One trip per material id at a time: a second request while one is in
    // flight returns the SAME future (prevents double-destroy of the session's
    // old material and orphaned builds).
    std::unordered_map<std::string, std::shared_future<std::string>> m_inflight;
    std::unordered_map<std::string, fs_cache_entry> m_fs_registry;
    bool m_fs_registry_dirty = false;
    bool m_fs_registry_loaded = false;
};

}  // namespace ui
}  // namespace kryga
