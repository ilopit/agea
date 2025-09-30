#pragma once

#include <core/reflection/reflection_type_utils.h>

namespace agea::root::custom
{
result_code
load_smart_object(blob_ptr ptr,
                  const serialization::conteiner& jc,
                  core::object_load_context& occ,
                  core::architype a_type);

result_code copy_smart_obj(AGEA_copy_handler_args);
result_code compare_smart_obj(AGEA_compare_handler_args);
result_code serialize_smart_obj(AGEA_serialization_args);
result_code deserialize_smart_obj(AGEA_deserialization_args);
result_code deserialize_from_proto_smart_obj(AGEA_deserialization_update_args);
result_code to_string_smart_obj(AGEA_reflection_type_ui_args);

result_code serialize_t_id(AGEA_serialization_args);
result_code deserialize_t_id(AGEA_deserialization_args);
result_code to_string_t_id(AGEA_reflection_type_ui_args);

result_code to_string_t_vec2(AGEA_reflection_type_ui_args);
result_code serialize_t_vec3(AGEA_serialization_args);
result_code deserialize_t_vec3(AGEA_deserialization_args);
result_code to_string_t_vec3(AGEA_reflection_type_ui_args);
result_code to_string_t_vec4(AGEA_reflection_type_ui_args);

result_code serialize_t_buf(AGEA_serialization_args);
result_code deserialize_t_buf(AGEA_deserialization_args);
result_code copy_t_buf(AGEA_copy_handler_args);
result_code to_string_t_buf(AGEA_reflection_type_ui_args);

}  // namespace agea::root::custom