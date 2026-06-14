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
KRG_ar_class(
    render_cmd_builder   = mesh_component__cmd_builder,
    render_cmd_destroyer = mesh_component__cmd_destroyer,
    render_cmd_transform = mesh_component__cmd_transform,
    mcp_hint             = "Renders 3D geometry — holds references to a mesh asset and a material "
                           "asset. Inherits transform from game_object_component"
);
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

protected:
    // clang-format off
    KRG_ar_property(
        category     = "Assets",
        serializable = true,
        check        = not_same,
        invalidates  = render,
        access       = all,
        default      = true,
        instantiate  = share,
        mcp_hint     = "surface appearance: colors / textures / shading — inspect/edit via "
                       "kryga_model_get_all with the material ID"
    );
    ::kryga::root::material* m_material = nullptr;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Assets",
        serializable = true,
        check        = not_same,
        invalidates  = render,
        access       = all,
        default      = true,
        instantiate  = share,
        mcp_hint     = "geometry data: vertices / triangles — swap by setting a different mesh ID"
    );
    ::kryga::root::mesh* m_mesh = nullptr;
    // clang-format on
};

}  // namespace base
}  // namespace kryga
