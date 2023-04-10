#pragma once

#include "root/mesh_component.generated.h"

#include "root/components/game_object_component.h"

namespace agea
{
namespace root
{
class game_object;
class mesh;
class material;

struct render_data;

AGEA_ar_class();
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

    bool
    construct(construct_params& c);

    material*
    get_material() const
    {
        return m_material;
    }

    void
    set_material(material* v)
    {
        AGEA_check(v, "Should not be NULL!");
        m_material = v;
        mark_render_dirty();
    }

    void
    set_mesh(mesh* v)
    {
        AGEA_check(v, "Should not be NULL!");
        m_mesh = v;
        mark_render_dirty();
    }

    mesh*
    get_mesh() const
    {
        return m_mesh;
    }

protected:
    AGEA_ar_property("category=assets", "serializable=true", "access=no", "default=true");
    material* m_material = nullptr;

    AGEA_ar_property("category=assets", "serializable=true", "access=no", "default=true");
    mesh* m_mesh = nullptr;
};

}  // namespace root
}  // namespace agea
