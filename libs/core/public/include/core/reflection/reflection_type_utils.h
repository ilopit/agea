#pragma once

#include <core/reflection/reflection_type.h>

#include <serialization/serialization.h>

namespace agea
{
namespace reflection
{
namespace utils
{

template <typename T>
T&
as_type(blob_ptr ptr)
{
    return *(T*)(ptr);
}

template <typename T>
void
pack_field(blob_ptr ptr, serialization::container& jc)
{
    jc = as_type<T>(ptr);
}

template <typename T>
void
extract_field(blob_ptr ptr, const serialization::container& jc)
{
    as_type<T>(ptr) = jc.as<T>();
}

inline blob_ptr
reduce_ptr(blob_ptr ptr, bool is_ptr)
{
    return is_ptr ? *(blob_ptr*)(ptr) : ptr;
}

template <typename T>
void
cpp_default__copy(blob_ptr from, blob_ptr to)
{
    as_type<T>(to) = as_type<T>(from);
}

template <typename T>
result_code
cpp_default__compare(blob_ptr from, blob_ptr to)
{
    return (as_type<T>(to) == as_type<T>(from)) ? result_code::ok : result_code::failed;
}

template <typename T>
result_code
cpp_default__copy(type_context__copy& ctx)
{
    reflection::utils::as_type<T>(ctx.dst_obj) = reflection::utils::as_type<T>(ctx.src_obj);
    return result_code::ok;
}

template <typename T>
result_code
cpp_default__save(type_context__save& ctx)
{
    reflection::utils::pack_field<T>(ctx.obj, *ctx.jc);
    return result_code::ok;
}

template <typename T>
result_code
cpp_default__to_string(type_context__to_string& ctx)
{
    auto& t = reflection::utils::as_type<T>(ctx.obj);
    *ctx.result = std::format("{}", t);
    return result_code::ok;
}

template <typename T>
result_code
cpp_default__load(type_context__load& ctx)
{
    reflection::utils::extract_field<T>(ctx.obj, *ctx.jc);
    return result_code::ok;
}

template <typename T>
result_code
cpp_default__compare(type_context__compare& ctx)
{
    return (as_type<T>(ctx.right_obj) == as_type<T>(ctx.left_obj)) ? result_code::ok
                                                                   : result_code::failed;
}

}  // namespace utils
}  // namespace reflection
}  // namespace agea
