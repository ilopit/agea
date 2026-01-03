#pragma once

#include "packages/base/model/mesh_component.ar.h"

#include "packages/root/model/components/game_object_component.h"

namespace kryga
{
namespace root
{
class mesh;
class material;

}  // namespace root

namespace base
{
// clang-format off
KRG_ar_class(render_constructor = mesh_component__render_loader,
              render_destructor  = mesh_component__render_destructor);
class mesh_component : public ::kryga::root::game_object_component
// clang-format on
{
    KRG_gen_meta__mesh_component();

public:
    KRG_gen_class_meta(mesh_component, ::kryga::root::game_object_component);

    KRG_gen_construct_params
    {
        ::kryga::root::mesh* mesh_handle = nullptr;
        ::kryga::root::material* material_handle = nullptr;
    };
    KRG_gen_meta_api;

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
    KRG_ar_property("category=Assets",
                     "serializable=true",
                     "check=not_same",
                     "invalidates=render",
                     "access=all",
                     "default=true");
    ::kryga::root::material* m_material = nullptr;

    KRG_ar_property("category=Assets",
                     "serializable=true",
                     "check=not_same",
                     "invalidates=render",
                     "access=all",
                     "default=true");
    ::kryga::root::mesh* m_mesh = nullptr;

    render::vulkan_render_data* m_render_handle = nullptr;
};

}  // namespace base
}  // namespace kryga
