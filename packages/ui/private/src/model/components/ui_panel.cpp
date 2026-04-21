#include "packages/ui/model/components/ui_panel.h"

namespace kryga
{
namespace ui
{

KRG_gen_class_cd_default(ui_panel);

bool
ui_panel::construct(construct_params& c)
{
    KRG_return_false(base_class::construct(c));

    if (c.material_handle)
    {
        m_material = c.material_handle;
    }

    return true;
}

}  // namespace ui
}  // namespace kryga
