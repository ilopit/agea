#pragma once

#include "model/components/level_object_component.h"

namespace agea
{
namespace model
{
class level_object;
class mesh;
class material;

struct render_data;

class mesh_component : public level_object_component
{
public:
    AGEA_gen_class_meta(mesh_component, level_object_component);

    AGEA_gen_construct_params
    {
        std::string material_id;
        std::string mesh_id;
    };
    AGEA_gen_meta_api;

    virtual bool prepare_for_rendering() override;

    bool construct(construct_params& c);

    bool clone(this_class& src);

    // protected:
    bool deserialize(json_conteiner& c);

    std::string m_material_id;
    std::string m_mesh_id;

    std::shared_ptr<mesh> m_mesh;
    std::shared_ptr<material> m_material;
};

}  // namespace model
}  // namespace agea
