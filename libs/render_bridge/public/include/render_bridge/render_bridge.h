#pragma once

#include "render_bridge/render_dependency.h"
#include "render_bridge/render_command.h"
#include "render_bridge/render_command_processor.h"

#include <packages/root/model/assets/shader_effect.h>

#include <utils/path.h>
#include <utils/id.h>
#include <utils/dynamic_object.h>
#include <utils/spsc_queue.h>
#include <utils/check.h>

#include <vulkan_render/vulkan_render_loader_create_infos.h>

#include <core/model_minimal.h>

#include <unordered_map>
#include <unordered_set>

namespace kryga
{
namespace root
{
class smart_object;
}

namespace render
{
class material_data;
class mesh_data;
struct vertex_input_description;
}  // namespace render

struct access_template
{
    std::shared_ptr<utils::dynobj_layout> layout;
    std::vector<uint32_t> offset_in_object;
};

class render_bridge
{
public:
    static render::shader_effect_create_info
    make_se_ci(root::shader_effect& se_model);

    static std::string
    make_qid(render::material_data& mt_data, render::mesh_data& m_data);

    // Model-side queue ID computation (no render-side pointers needed)
    static std::string
    make_qid_from_model(root::smart_object& mat_model, root::smart_object& mesh_model);

    static bool
    is_kryga_texture(const utils::path& p);

    static bool
    is_kryga_mesh(const utils::path& p);

    kryga::result_code
    render_cmd_build(root::smart_object& obj, bool sub_objects);

    kryga::result_code
    render_cmd_destroy(root::smart_object& obj, bool sub_objects);

    utils::dynobj
    collect_gpu_data(root::smart_object& so);

    utils::dynobj
    extract_gpu_data(root::smart_object& so, const access_template& t);

    // Set texture/sampler bindings in material GPU data (at fixed offsets at start of buffer)
    static void
    set_material_texture_bindings(utils::dynobj& gpu_data,
                                  const uint32_t* texture_indices,
                                  const uint32_t* sampler_indices,
                                  uint32_t slot_count);

    render_object_dependency_graph&
    get_dependency()
    {
        return m_dependency_graph;
    }

    // Allocate a command from the per-frame arena (main thread)
    template <typename T, typename... Args>
    T*
    alloc_cmd(Args&&... args)
    {
        return m_arena.alloc<T>(std::forward<Args>(args)...);
    }

    // Enqueue a command for deferred processing by the render thread
    void
    enqueue_cmd(render_cmd::render_command_base* cmd);

    // Drain the command queue: execute + dtor each command (called from render thread)
    void
    drain_queue();

    // Reset the arena after render thread is done (called from main thread)
    void
    reset_arena();

    render_command_processor&
    get_processor()
    {
        return m_processor;
    }

private:
    std::unordered_map<utils::id, access_template> m_gpu_data_collection_templates;

    render_object_dependency_graph m_dependency_graph;
    render_command_processor m_processor;
    render_cmd::cmd_arena m_arena;
    utils::spsc_queue<render_cmd::render_command_base*> m_command_queue{16384};
};

}  // namespace kryga
