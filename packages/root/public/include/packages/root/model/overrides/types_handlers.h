#pragma once

#include <core/reflection/reflection_type_utils.h>

AGEA_ar_model_overrides();
namespace agea::root
{
result_code
load_smart_object(blob_ptr ptr,
                  const serialization::conteiner& jc,
                  core::object_load_context& occ,
                  core::architype a_type);

result_code color__serialize(AGEA_serialization_args);
result_code color__deserialize(AGEA_deserialization_args);
result_code color__copy(AGEA_copy_handler_args);
result_code color__to_string(AGEA_reflection_type_ui_args);

result_code buffer__serialize(AGEA_serialization_args);
result_code buffer__deserialize(AGEA_deserialization_args);
result_code buffer__copy(AGEA_copy_handler_args);
result_code buffer__to_string(AGEA_reflection_type_ui_args);

result_code id__serialize(AGEA_serialization_args);
result_code id__deserialize(AGEA_deserialization_args);
result_code id__to_string(AGEA_reflection_type_ui_args);

result_code smart_obj__copy(AGEA_copy_handler_args);
result_code smart_obj__compare(AGEA_compare_handler_args);
result_code smart_obj__serialize(AGEA_serialization_args);
result_code smart_obj__deserialize(AGEA_deserialization_args);
result_code smart_obj__deserialize_from_proto(AGEA_deserialization_update_args);
result_code smart_obj__to_string(AGEA_reflection_type_ui_args);

result_code vec2__to_string(AGEA_reflection_type_ui_args);

result_code vec3__serialize(AGEA_serialization_args);
result_code vec3__deserialize(AGEA_deserialization_args);
result_code vec3__to_string(AGEA_reflection_type_ui_args);

result_code vec4__to_string(AGEA_reflection_type_ui_args);

}  // namespace agea::root