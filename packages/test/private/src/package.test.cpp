#include "packages/test/package.test.h"

#include <core/package_manager.h>

namespace agea::test
{

package::package()
    : core::package(AID("test"))
{
}

}  // namespace agea::test