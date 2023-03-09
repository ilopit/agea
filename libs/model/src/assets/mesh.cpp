#include "model/assets/mesh.h"

namespace agea
{
namespace model
{

AGEA_gen_class_cd_default(mesh);

bool
mesh::construct(this_class::construct_params& params)
{
    if (!base_class::construct(params))
    {
        return false;
    }

    m_indices = params.indices;
    m_vertices = params.vertices;

    return true;
}

}  // namespace model
}  // namespace agea
