#pragma once

#include "packages/ui/model/ui_panel.ar.h"

#include "packages/ui/model/ui_widget.h"
#include "packages/root/model/core_types/vec3.h"

namespace kryga
{
namespace ui
{

// A solid-color rectangle.
//
// Positioned by the fields on ui_widget (pixel space, top-left origin).
// Color is per-instance and travels to the GPU as a push constant — no
// material asset, no shader_effect asset, no bindless textures involved.
// clang-format off
KRG_ar_class(render_cmd_builder   = ui_panel__cmd_builder,
             render_cmd_destroyer = ui_panel__cmd_destroyer);
class ui_panel : public ui_widget
// clang-format on
{
    KRG_gen_meta__ui_panel();

public:
    KRG_gen_class_meta(ui_panel, ui_widget);

    KRG_gen_construct_params
    {
    };

    KRG_gen_meta_api;

    bool
    construct(construct_params& c);

    const ::kryga::root::vec3&
    get_color() const
    {
        return m_color;
    }
    float
    get_opacity() const
    {
        return m_opacity;
    }

protected:
    KRG_ar_property("category=Appearance",
                    "serializable=true",
                    "access=all",
                    "invalidates=render",
                    "default=true");
    ::kryga::root::vec3 m_color = {0.2f, 0.6f, 0.9f};

    KRG_ar_property("category=Appearance",
                    "serializable=true",
                    "access=all",
                    "invalidates=render",
                    "default=true");
    float m_opacity = 1.0f;
};

}  // namespace ui
}  // namespace kryga
