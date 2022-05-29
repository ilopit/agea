#pragma once

#include "reflection/property_utils.h"

namespace agea
{
namespace reflection
{
namespace custom
{
bool
deserialize_game_object_components(deserialize_context& dc);

bool
serialize_game_object_components(serialize_context& dc);
}  // namespace custom
}  // namespace reflection
}  // namespace agea