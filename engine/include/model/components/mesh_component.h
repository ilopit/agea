#pragma once

#include "model/components/game_object_component.h"

namespace agea
{
namespace model
{
class game_object;
class mesh;
class material;

struct render_data;

class mesh_component : public game_object_component
{
public:
    AGEA_gen_class_meta(mesh_component, game_object_component);

    AGEA_gen_construct_params
    {
        std::string material_id;
        std::string mesh_id;
    };
    AGEA_gen_meta_api;

    virtual bool
    prepare_for_rendering() override;

    bool
    construct(construct_params& c);

    AGEA_property("category=assets", "serializable=true", "access=rw", "visible=true");
    material* m_material = nullptr;

    AGEA_property("category=assets", "serializable=true", "access=rw", "visible=true");
    mesh* m_mesh = nullptr;
};

}  // namespace model
}  // namespace agea
