#pragma once

#include <model/model_fwds.h>

#include <utils/id.h>
#include <utils/dynamic_object.h>

namespace agea
{
class scene_builder
{
    using pfr = bool (scene_builder::*)(root::smart_object&, bool);

public:
    scene_builder();

    bool
    prepare_for_rendering(root::smart_object& obj, bool sub_objects);

private:
    // clang-format off
    bool pfr_mesh(root::smart_object& obj, bool sub_objects);
    bool pfr_material(root::smart_object& obj, bool sub_objects);
    bool pfr_texture(root::smart_object& obj, bool sub_objects);
    bool pfr_game_object(root::smart_object& obj, bool sub_objects);
    bool pfr_game_object_component(root::smart_object& obj, bool sub_objects);
    bool pfr_mesh_component(root::smart_object& obj, bool sub_objects);
    bool pfr_shader_effect(root::smart_object& obj, bool sub_objects);
    bool pfr_empty(root::smart_object& obj, bool sub_objects);

    // clang-format on

    utils::dynamic_object
    collect_gpu_data(root::smart_object& so);

    struct collection_template
    {
        std::shared_ptr<utils::dynamic_object_layout> layout;
        std::vector<uint32_t> offset_in_object;
    };

    utils::dynamic_object
    extract_gpu_data(root::smart_object& so, const scene_builder::collection_template& t);

    bool
    create_collection_template(root::smart_object& so, scene_builder::collection_template& t);

    std::unordered_map<utils::id, pfr> m_pfr_handlers;

    std::unordered_map<utils::id, collection_template> m_gpu_data_collection_templates;
};

}  // namespace agea