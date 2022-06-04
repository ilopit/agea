#pragma once

#include "reflection/property_utils.h"

namespace agea
{
namespace reflection
{
namespace custom
{
bool
game_object_components_deserialize(deserialize_context& dc);

bool
game_object_components_serialize(serialize_context& dc);

bool
game_object_components_compare(compare_context& ctx);

bool
game_object_components_copy(copy_context& ctx);

}  // namespace custom
}  // namespace reflection
}  // namespace agea