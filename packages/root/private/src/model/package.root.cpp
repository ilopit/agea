#include "packages/root/package.root.h"

#include <core/package_manager.h>

namespace kryga::root
{

package::package()
    : core::package(AID("root"))
{
}

}  // namespace kryga::root