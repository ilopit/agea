#pragma once

#include "core/reflection/property_utils.h"

AGEA_ar_model_overrides();
namespace agea::root
{
// clang-format off
result_code game_object_components_deserialize(reflection::deserialize_context& dc);
result_code game_object_components_serialize(reflection::serialize_context& dc);
result_code game_object_components_compare(reflection::compare_context& ctx);
result_code game_object_components_copy(reflection::copy_context& ctx);
result_code game_object_load_derive(reflection::property_load_derive_context& ctx);

result_code property_texture_sample__deserialize(reflection::deserialize_context& dc);
result_code property_texture_sample__serialize(reflection::serialize_context& dc);
result_code property_texture_sample__compare(reflection::compare_context& ctx);
result_code property_texture_sample__copy(reflection::copy_context& ctx);
result_code property_texture_sample__instantiate(reflection::instantiate_context& ctx);
result_code property_texture_sample__load_derive(reflection::property_load_derive_context& ctx);

}  // namespace agea::root
