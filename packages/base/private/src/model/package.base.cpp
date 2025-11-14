#include "packages/base/package.base.h"

#include <core/package_manager.h>

namespace agea::base
{

package::package()
    : core::static_package(AID("base"))
{
}

}  // namespace agea::base