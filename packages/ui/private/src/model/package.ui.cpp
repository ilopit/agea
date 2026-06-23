#include "packages/ui/package.ui.h"

#include <core/package_manager.h>

namespace kryga::ui
{

package::package()
    : core::package(AID("ui"))
{
}

}  // namespace kryga::ui
