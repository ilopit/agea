#include "packages/root/package.root.h"

#include <core/package_manager.h>

namespace agea::root
{

package::package()
    : core::package(AID("root"))
{
}

}  // namespace agea::root