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
        mesh* mesh_handle = nullptr;
        material* material_handle = nullptr;
    };
    AGEA_gen_meta_api;

    bool
    construct(construct_params& c);

    render::vulkan_render_data*
    get_render_object_data() const
    {
        return m_render_handle;
    }
    void
    set_render_object_data(render::vulkan_render_data* v)
    {
        m_render_handle = v;
    }

protected:
    AGEA_ar_property("category=assets",
                     "serializable=true",
                     "check=not_same",
                     "invalidates=render",
                     "access=all",
                     "default=true");
    material* m_material = nullptr;

    AGEA_ar_property("category=assets",
                     "serializable=true",
                     "check=not_same",
                     "invalidates=render",
                     "access=all",
                     "default=true");
    mesh* m_mesh = nullptr;

    render::vulkan_render_data* m_render_handle = nullptr;
};

}  // namespace root
}  // namespace agea
