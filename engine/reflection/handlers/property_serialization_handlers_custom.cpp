#pragma once

#include "reflection/handlers/property_serialization_handlers_custom.h"
#include "utils/agea_log.h"
#include "model/object_construction_context.h"

namespace agea
{
namespace reflection
{
namespace custom
{
bool
handle_object_components(AGEA_deserialization_args)
{
    auto& field = reflection::extract<std::vector<::agea::model::smart_object*>>(ptr);

    return true;
}
}  // namespace custom
}  // namespace reflection

}  // namespace agea