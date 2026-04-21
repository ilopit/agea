#pragma once

#include "packages/ui/model/ui_panel.ar.h"

#include "packages/root/model/components/game_object_component.h"

namespace kryga
{
namespace root
{
class material;
}  // namespace root

namespace ui
{
// A 2D screen-space rectangle.
//
// Transform semantics in this first iteration: the inherited
// game_object_component position / scale are interpreted directly as NDC
// (normalized device coordinates, x/y in [-1, 1], (0,0) = screen center,
// +y = up). The underlying plane_mesh spans (-1,-1)..(1,1), so:
//     final_ndc = position.xy + in_vertex.xy * scale.xy
// i.e. `scale = (half_width_ndc, half_height_ndc, 1)`.
//
// Pixel-to-NDC conversion and anchoring belong to a later iteration.
// clang-format off
KRG_ar_class(render_cmd_builder   = ui_panel__cmd_builder,
             render_cmd_destroyer = ui_panel__cmd_destroyer);
class ui_panel : public ::kryga::root::game_object_component
// clang-format on
{
    KRG_gen_meta__ui_panel();

public:
    KRG_gen_class_meta(ui_panel, ::kryga::root::game_object_component);

    KRG_gen_construct_params
    {
        ::kryga::root::material* material_handle = nullptr;
    };
    KRG_gen_meta_api;

    bool
    construct(construct_params& c);

    ::kryga::root::material*
    get_material() const
    {
        return m_material;
    }

protected:
    KRG_ar_property("category=Assets",
                    "serializable=true",
                    "check=not_same",
                    "invalidates=render",
                    "access=all",
                    "default=true");
    ::kryga::root::material* m_material = nullptr;
};

}  // namespace ui
}  // namespace kryga
