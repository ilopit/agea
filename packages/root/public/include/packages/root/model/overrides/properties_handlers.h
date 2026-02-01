#pragma once

#include "core/reflection/property_utils.h"

KRG_ar_model_overrides();
namespace kryga::root
{
// clang-format off
result_code game_object_components_save(reflection::property_context__save& dc);
result_code game_object_components_compare(reflection::property_context__compare& ctx);
result_code game_object_components_copy(reflection::property_context__copy& ctx);
result_code game_object_components_instantiate(reflection::property_context__instantiate& ctx);
result_code game_object_components__load(reflection::property_context__load& ctx);

result_code property_texture_slot__save(reflection::property_context__save& dc);
result_code property_texture_slot__compare(reflection::property_context__compare& ctx);
result_code property_texture_slot__copy(reflection::property_context__copy& ctx);
result_code property_texture_slot__instantiate(reflection::property_context__instantiate& ctx);
result_code property_texture_slot__load(reflection::property_context__load& ctx);

}  // namespace kryga::root
