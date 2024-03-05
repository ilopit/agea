#include "packages/root/package.root.h"

#include <core/package_manager.h>

namespace agea::root
{

namespace
{
package_autoregister<root::package> s_package;

}

package::package()
    : core::static_package(AID("root"))
{
}

}  // namespace agea::root