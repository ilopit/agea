#include "packages/base/package.base.h"

#include <core/package_manager.h>

namespace agea::base
{

package::package()
    : core::package(AID("base"))
{
}

}  // namespace agea::base