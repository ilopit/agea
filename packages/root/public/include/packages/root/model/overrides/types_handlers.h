#pragma once

#include <core/reflection/reflection_type_utils.h>

AGEA_ar_model_overrides();
namespace agea::root
{
// clang-format off

result_code
load_smart_object(blob_ptr ptr,
                  const serialization::conteiner& jc,
                  core::object_load_context& occ,
                  core::architype a_type);

result_code color__serialize(reflection::type_serialization_context& ctx);
result_code color__load_derive(reflection::type_load_derive_context& ctx);
result_code color__copy(reflection::type_copy_context& ctx);
result_code color__to_string(reflection::type_ui_context& ctx);

result_code buffer__serialize(reflection::type_serialization_context& ctx);
result_code buffer__load_derive(reflection::type_load_derive_context& ctx);
result_code buffer__copy(reflection::type_copy_context& ctx);
result_code buffer__to_string(reflection::type_ui_context& ctx);

result_code id__serialize(reflection::type_serialization_context& ctx);
result_code id__load_derive(reflection::type_load_derive_context& ctx);
result_code id__to_string(reflection::type_ui_context& ctx);

result_code smart_obj__copy(reflection::type_copy_context& ctx);
result_code smart_obj__instantiate(reflection::type_copy_context& ctx);
result_code smart_obj__compare(reflection::type_compare_context& ctx);
result_code smart_obj__load_derive(reflection::type_load_derive_context& ctx);
result_code smart_obj__serialize(reflection::type_serialization_context& ctx);
result_code smart_obj__to_string(reflection::type_ui_context& ctx);

result_code texture_sample__serialize(reflection::type_serialization_context& ctx);
result_code texture_sample__compare(reflection::type_compare_context& ctx);
result_code texture_sample__copy(reflection::type_copy_context& ctx);
result_code texture_sample__instantiate(reflection::type_copy_context& ctx);
result_code texture_sample__load_derive(reflection::type_load_derive_context& ctx);

result_code vec2__to_string(reflection::type_ui_context& ctx);

result_code vec3__serialize(reflection::type_serialization_context& ctx);
result_code vec3__load_derive(reflection::type_load_derive_context& ctx);
result_code vec3__to_string(reflection::type_ui_context& ctx);

result_code vec4__to_string(reflection::type_ui_context& ctx);

// clang-format on

}  // namespace agea::root