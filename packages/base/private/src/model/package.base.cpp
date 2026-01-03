#include "packages/base/package.base.h"

#include <core/package_manager.h>

namespace kryga::base
{

package::package()
    : core::package(AID("base"))
{
}

}  // namespace kryga::base