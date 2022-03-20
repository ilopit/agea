#include "level_object.h"

#include "model/mesh_object.h"
#include "model/components/mesh_component.h"

#include <json/json.h>

namespace agea
{
namespace model
{
namespace
{
}  // namespace

bool
mesh_object::clone(this_class& src)
{
    base_class::clone(src);

    return true;
}

bool
mesh_object::construct(this_class::construct_params& p)
{
    base_class::construct(p);

    return true;
}

bool
mesh_object::deserialize(json_conteiner& c)
{
    base_class::deserialize(c);

    smart_object_serializer::components_deserialize(c, *this);

    return true;
}

bool
mesh_object::deserialize_finalize(json_conteiner& c)
{
    base_class::deserialize_finalize(c);

    return true;
}

}  // namespace model
}  // namespace agea
