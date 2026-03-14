#pragma once

#include "render_bridge/render_dependency.h"
#include "render_bridge/render_command.h"

#include <packages/root/model/assets/shader_effect.h>

#include <utils/path.h>
#include <utils/id.h>
#include <utils/dynamic_object.h>
#include <utils/check.h>

#include <vulkan_render/vulkan_render_loader_create_infos.h>

#include <core/model_minimal.h>
#include <core/queues.h>
#include <global_state/global_state.h>

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

    // Convenience: allocate a command from the per-frame arena
    template <typename T, typename... Args>
    T*
    alloc_cmd(Args&&... args);

    // Convenience: enqueue a command for render thread
    void
    enqueue_cmd(render_cmd::render_command_base* cmd);

    // Drain the command queue (called from render thread)
    void
    drain_queue();

    // Reset the arena (called from main thread after render done)
    void
    reset_arena();

private:
    std::unordered_map<utils::id, access_template> m_gpu_data_collection_templates;

    render_object_dependency_graph m_dependency_graph;
};

template <typename T, typename... Args>
T*
render_bridge::alloc_cmd(Args&&... args)
{
    return glob::glob_state().getr_queues().get_render().alloc_cmd<T>(
        std::forward<Args>(args)...);
}

}  // namespace kryga
