#include "packages/ui/model/ui_text.h"

namespace kryga
{
namespace ui
{

KRG_gen_class_cd_default(ui_text);

bool
ui_text::construct(construct_params& c)
{
    KRG_return_false(base_class::construct(c));
    return true;
}

}  // namespace ui
}  // namespace kryga
