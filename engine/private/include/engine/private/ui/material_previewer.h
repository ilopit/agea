#pragma once

#include <vulkan_render/utils/offscreen_renderer.h>
#include <vulkan_render/types/vulkan_render_types_fwds.h>

#include <utils/id.h>

#include <cstdint>
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
};

class material_previewer
{
public:
    std::string
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

    render::material_data*
    ensure_gpu_material(const utils::id& material_id);

    render::offscreen_renderer m_renderer;
    std::unordered_map<std::string, edit_session> m_sessions;
    std::unordered_map<std::string, memory_cache_entry> m_memory_cache;
    std::unordered_map<std::string, fs_cache_entry> m_fs_registry;
    bool m_fs_registry_dirty = false;
    bool m_fs_registry_loaded = false;
};

}  // namespace ui
}  // namespace kryga
