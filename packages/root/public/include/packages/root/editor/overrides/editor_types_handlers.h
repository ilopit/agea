#pragma once

#include <core/reflection/reflection_type_utils.h>

KRG_ar_editor_overrides();
namespace kryga::root
{
// clang-format off

result_code bool__json_save(reflection::type_context__json_save& ctx);
result_code bool__json_load(reflection::type_context__json_load& ctx);
result_code int8__json_save(reflection::type_context__json_save& ctx);
result_code int8__json_load(reflection::type_context__json_load& ctx);
result_code int16__json_save(reflection::type_context__json_save& ctx);
result_code int16__json_load(reflection::type_context__json_load& ctx);
result_code int32__json_save(reflection::type_context__json_save& ctx);
result_code int32__json_load(reflection::type_context__json_load& ctx);
result_code int64__json_save(reflection::type_context__json_save& ctx);
result_code int64__json_load(reflection::type_context__json_load& ctx);
result_code uint8__json_save(reflection::type_context__json_save& ctx);
result_code uint8__json_load(reflection::type_context__json_load& ctx);
result_code uint16__json_save(reflection::type_context__json_save& ctx);
result_code uint16__json_load(reflection::type_context__json_load& ctx);
result_code uint32__json_save(reflection::type_context__json_save& ctx);
result_code uint32__json_load(reflection::type_context__json_load& ctx);
result_code uint64__json_save(reflection::type_context__json_save& ctx);
result_code uint64__json_load(reflection::type_context__json_load& ctx);
result_code float__json_save(reflection::type_context__json_save& ctx);
result_code float__json_load(reflection::type_context__json_load& ctx);
result_code double__json_save(reflection::type_context__json_save& ctx);
result_code double__json_load(reflection::type_context__json_load& ctx);
result_code string__json_save(reflection::type_context__json_save& ctx);
result_code string__json_load(reflection::type_context__json_load& ctx);

result_code id__json_save(reflection::type_context__json_save& ctx);
result_code id__json_load(reflection::type_context__json_load& ctx);

result_code vec2__json_save(reflection::type_context__json_save& ctx);
result_code vec2__json_load(reflection::type_context__json_load& ctx);
result_code vec3__json_save(reflection::type_context__json_save& ctx);
result_code vec3__json_load(reflection::type_context__json_load& ctx);
result_code vec4__json_save(reflection::type_context__json_save& ctx);
result_code vec4__json_load(reflection::type_context__json_load& ctx);

// clang-format on

}  // namespace kryga::root
