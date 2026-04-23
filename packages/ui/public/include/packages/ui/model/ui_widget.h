#pragma once

#include "packages/ui/model/ui_widget.ar.h"

#include "packages/root/model/smart_object.h"

namespace kryga
{
namespace ui
{

// Base class for retained-mode UI widgets.
//
// A widget is a reflected smart_object (so it participates in package /
// level serialization) but deliberately does NOT inherit from
// game_object_component — UI has no 3D transform, no world position, no
// frustum culling, no layer flags, no render-children. All widgets live in
// pixel space.
//
// Fields are in screen pixels with origin at the top-left of the viewport.
// The render handler converts to NDC once per update using the current
// viewport size.
KRG_ar_class();
class ui_widget : public ::kryga::root::smart_object
{
    KRG_gen_meta__ui_widget();

public:
    KRG_gen_class_meta(ui_widget, ::kryga::root::smart_object);

    KRG_gen_construct_params
    {
    };

    KRG_gen_meta_api;

    bool
    construct(construct_params& c);

    int32_t
    get_x() const
    {
        return m_x;
    }
    int32_t
    get_y() const
    {
        return m_y;
    }
    int32_t
    get_width() const
    {
        return m_width;
    }
    int32_t
    get_height() const
    {
        return m_height;
    }
    bool
    is_visible() const
    {
        return m_visible;
    }

protected:
    KRG_ar_property("category=Layout",
                    "serializable=true",
                    "access=all",
                    "invalidates=render",
                    "default=true");
    int32_t m_x = 0;

    KRG_ar_property("category=Layout",
                    "serializable=true",
                    "access=all",
                    "invalidates=render",
                    "default=true");
    int32_t m_y = 0;

    KRG_ar_property("category=Layout",
                    "serializable=true",
                    "access=all",
                    "invalidates=render",
                    "default=true");
    int32_t m_width = 100;

    KRG_ar_property("category=Layout",
                    "serializable=true",
                    "access=all",
                    "invalidates=render",
                    "default=true");
    int32_t m_height = 100;

    KRG_ar_property("category=Layout",
                    "serializable=true",
                    "access=all",
                    "invalidates=render",
                    "default=true");
    bool m_visible = true;
};

}  // namespace ui
}  // namespace kryga
