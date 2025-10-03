#include "packages/root/package.root.h"

#include <core/global_state.h>

#include <utils/static_initializer.h>

namespace agea::root
{

AGEA_schedule_static_register(
    [](core::state&)
    { package::instance().register_package_extention<package::package_object_builder>(); });

bool
package::package_object_builder::build(core::static_package& s)
{
    return true;
}

}  // namespace agea::root