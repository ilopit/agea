#include "packages/tbs/model/hex_grid.h"
#include "packages/tbs/model/hex_tile.h"

namespace kryga
{
namespace tbs
{

KRG_gen_class_cd_default(hex_grid);

bool
hex_grid::construct(construct_params& params)
{
    if (!base_class::construct(params))
    {
        return false;
    }

    return true;
}

}  // namespace tbs
}  // namespace kryga
