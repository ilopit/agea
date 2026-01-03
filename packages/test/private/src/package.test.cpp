#include "packages/test/package.test.h"

#include <core/package_manager.h>

namespace kryga::test
{

package::package()
    : core::package(AID("test"))
{
}

}  // namespace kryga::test