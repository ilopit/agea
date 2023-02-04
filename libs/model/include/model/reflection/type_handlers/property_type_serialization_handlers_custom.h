#pragma once

#include "model/reflection/property_utils.h"

namespace agea
{
namespace reflection
{
namespace custom
{
result_code
game_object_components_deserialize(deserialize_context& dc);

result_code
game_object_components_prototype(property_prototype_context& dc);

result_code
game_object_components_serialize(serialize_context& dc);

result_code
game_object_components_compare(compare_context& ctx);

result_code
game_object_components_copy(copy_context& ctx);

///
result_code
texture_sample_deserialize(deserialize_context& dc);

result_code
texture_sample_prototype(property_prototype_context& dc);

result_code
texture_sample_serialize(serialize_context& dc);

result_code
texture_sample_compare(compare_context& ctx);

result_code
texture_sample_copy(copy_context& ctx);

}  // namespace custom
}  // namespace reflection
}  // namespace agea