#pragma once

#include <core/reflection/reflection_type_utils.h>

AGEA_ar_model_overrides();
namespace agea::root
{
// clang-format off

result_code
load_smart_object(blob_ptr ptr,
                  const serialization::container& jc,
                  core::object_load_context& occ,
                  core::architype a_type);

result_code color__save(reflection::type_context__save& ctx);
result_code color__load(reflection::type_context__load& ctx);
result_code color__copy(reflection::type_context__copy& ctx);
result_code color__to_string(reflection::type_context__to_string& ctx);

result_code buffer__save(reflection::type_context__save& ctx);
result_code buffer__load(reflection::type_context__load& ctx);
result_code buffer__copy(reflection::type_context__copy& ctx);
result_code buffer__to_string(reflection::type_context__to_string& ctx);

result_code id__save(reflection::type_context__save& ctx);
result_code id__load(reflection::type_context__load& ctx);
result_code id__to_string(reflection::type_context__to_string& ctx);

result_code smart_obj__copy(reflection::type_context__copy& ctx);
result_code smart_obj__instantiate(reflection::type_context__copy& ctx);
result_code smart_obj__compare(reflection::type_context__compare& ctx);
result_code smart_obj__load(reflection::type_context__load& ctx);
result_code smart_obj__save(reflection::type_context__save& ctx);
result_code smart_obj__to_string(reflection::type_context__to_string& ctx);

result_code texture_sample__save(reflection::type_context__save& ctx);
result_code texture_sample__compare(reflection::type_context__compare& ctx);
result_code texture_sample__copy(reflection::type_context__copy& ctx);
result_code texture_sample__instantiate(reflection::type_context__copy& ctx);
result_code texture_sample__load(reflection::type_context__load& ctx);

result_code vec2__to_string(reflection::type_context__to_string& ctx);

result_code vec3__save(reflection::type_context__save& ctx);
result_code vec3__load(reflection::type_context__load& ctx);
result_code vec3__to_string(reflection::type_context__to_string& ctx);

result_code vec4__to_string(reflection::type_context__to_string& ctx);

// clang-format on

}  // namespace agea::root