#pragma once

#include "mesh_component.generated.h"

#include "model/components/game_object_component.h"

namespace agea
{
namespace model
{
class game_object;
class mesh;
class material;

struct render_data;

AGEA_class();
class mesh_component : public game_object_component
{
    AGEA_gen_meta__mesh_component();

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

protected:
    AGEA_property("category=assets", "serializable=true");
    material* m_material = nullptr;

    AGEA_property("category=assets", "serializable=true");
    mesh* m_mesh = nullptr;
};

}  // namespace model
}  // namespace agea
