#include "packages/test/package.test.h"

#include <core/package_manager.h>

namespace agea::test
{

package::package()
    : core::static_package(AID("test"))
{
}

}  // namespace agea::test