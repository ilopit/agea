#include "packages/base/render/render_resource.h"

#include <global_state/global_state.h>
#include <packages/base/package.base.h>

namespace kryga::base
{

bool
root_package_render_resources_loader::load(core::static_package& s)
{
    return true;
}

}  // namespace kryga::base