#pragma once

#include <core/reflection/reflection_type_utils.h>

KRG_ar_model_overrides();
namespace kryga::root
{
// clang-format off

result_code
load_smart_object(blob_ptr ptr,
                  const serialization::container& jc,
                  core::object_constructor& ctor,
                  core::architype a_type);


result_code buffer__save(reflection::type_context__save& ctx);
result_code buffer__load(reflection::type_context__load& ctx);
result_code buffer__copy(reflection::type_context__copy& ctx);
result_code buffer__compare(reflection::type_context__compare& ctx);

result_code id__save(reflection::type_context__save& ctx);
result_code id__load(reflection::type_context__load& ctx);

result_code smart_obj__copy(reflection::type_context__copy& ctx);
result_code smart_obj__instantiate(reflection::type_context__copy& ctx);
result_code smart_obj__compare(reflection::type_context__compare& ctx);
result_code smart_obj__load(reflection::type_context__load& ctx);
result_code smart_obj__save(reflection::type_context__save& ctx);

result_code texture_slot__save(reflection::type_context__save& ctx);
result_code texture_slot__compare(reflection::type_context__compare& ctx);
result_code texture_slot__copy(reflection::type_context__copy& ctx);
result_code texture_slot__instantiate(reflection::type_context__copy& ctx);
result_code texture_slot__load(reflection::type_context__load& ctx);

result_code vec3__save(reflection::type_context__save& ctx);
result_code vec3__load(reflection::type_context__load& ctx);

// clang-format on

}  // namespace kryga::root