#include "packages/root/package.root.h"

namespace agea::root
{

core::package_extention_autoregister<root::package, package::package_object_builder>
    s_custom_loader{};

bool
package::package_object_builder::build(core::static_package& s)
{
    return true;
}

}  // namespace agea::root