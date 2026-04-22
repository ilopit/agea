#include "packages/ui/model/ui_panel.h"

namespace kryga
{
namespace ui
{

KRG_gen_class_cd_default(ui_panel);

bool
ui_panel::construct(construct_params& c)
{
    KRG_return_false(base_class::construct(c));
    return true;
}

}  // namespace ui
}  // namespace kryga
