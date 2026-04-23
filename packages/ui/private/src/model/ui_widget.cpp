#include "packages/ui/model/ui_widget.h"

namespace kryga
{
namespace ui
{

KRG_gen_class_cd_default(ui_widget);

bool
ui_widget::construct(construct_params& c)
{
    KRG_return_false(base_class::construct(c));
    return true;
}

}  // namespace ui
}  // namespace kryga
