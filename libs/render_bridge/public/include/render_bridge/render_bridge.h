#pragma once

#include "render_bridge/render_dependency.h"

#include <packages/root/assets/shader_effect.h>

#include <utils/path.h>
#include <utils/id.h>
#include <utils/dynamic_object.h>
#include <utils/singleton_instance.h>

#include <vulkan_render/vulkan_render_loader_create_infos.h>

#include <core/model_minimal.h>

#include <unordered_map>
#include <unordered_set>

namespace agea
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

    static bool
    is_agea_texture(const utils::path& p);

    static bool
    is_agea_mesh(const utils::path& p);

    agea::result_code
    render_ctor(root::smart_object& obj, bool sub_objects);

    agea::result_code
    render_dtor(root::smart_object& obj, bool sub_objects);

    utils::dynobj
    collect_gpu_data(root::smart_object& so);

    utils::dynobj
    extract_gpu_data(root::smart_object& so, const access_template& t);

    bool
    create_collection_template(root::smart_object& so, access_template& t);

    render_object_dependency_graph&
    get_dependency()
    {
        return m_dependency_graph;
    }

private:
    std::unordered_map<utils::id, access_template> m_gpu_data_collection_templates;

    render_object_dependency_graph m_dependency_graph;
};

namespace glob
{
class render_bridge : public singleton_instance<::agea::render_bridge, render_bridge>
{
};
}  // namespace glob

}  // namespace agea