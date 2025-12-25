#pragma once

#include "core/reflection/property_utils.h"

AGEA_ar_model_overrides();
namespace agea::root
{
// clang-format off
result_code game_object_components_save(reflection::property_context__save& dc);
result_code game_object_components_compare(reflection::property_context__compare& ctx);
result_code game_object_components_copy(reflection::property_context__copy& ctx);
result_code game_object_components_instantiate(reflection::property_context__instantiate& ctx);
result_code game_object_components__load(reflection::property_context__load& ctx);

result_code property_texture_sample__save(reflection::property_context__save& dc);
result_code property_texture_sample__compare(reflection::property_context__compare& ctx);
result_code property_texture_sample__copy(reflection::property_context__copy& ctx);
result_code property_texture_sample__instantiate(reflection::property_context__instantiate& ctx);
result_code property_texture_sample__load(reflection::property_context__load& ctx);

}  // namespace agea::root
