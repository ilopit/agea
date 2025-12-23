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
pack_field(blob_ptr ptr, serialization::conteiner& jc)
{
    jc = as_type<T>(ptr);
}

template <typename T>
void
extract_field(blob_ptr ptr, const serialization::conteiner& jc)
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
cpp_default__copy(type_copy_context& ctx)
{
    reflection::utils::as_type<T>(ctx.to) = reflection::utils::as_type<T>(ctx.from);
    return result_code::ok;
}

template <typename T>
result_code
cpp_default__serialize(type_serialization_context& ctx)
{
    reflection::utils::pack_field<T>(ctx.ptr, *ctx.jc);
    return result_code::ok;
}

template <typename T>
result_code
cpp_default__to_string(type_ui_context& ctx)
{
    auto& t = reflection::utils::as_type<T>(ctx.ptr);
    *ctx.result = std::format("{}", t);
    return result_code::ok;
}

template <typename T>
result_code
cpp_default__load(type_load_derive_context& ctx)
{
    reflection::utils::extract_field<T>(ctx.ptr, *ctx.jc);
    return result_code::ok;
}

template <typename T>
result_code
cpp_default__compare(type_compare_context& ctx)
{
    return (as_type<T>(ctx.to) == as_type<T>(ctx.from)) ? result_code::ok : result_code::failed;
}

}  // namespace utils
}  // namespace reflection
}  // namespace agea
