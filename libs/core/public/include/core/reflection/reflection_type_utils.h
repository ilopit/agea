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
cpp_copy(blob_ptr from, blob_ptr to)
{
    as_type<T>(to) = as_type<T>(from);
}

template <typename T>
result_code
cpp_compare(blob_ptr from, blob_ptr to)
{
    return (as_type<T>(to) == as_type<T>(from)) ? result_code::ok : result_code::failed;
}

template <typename T>
result_code
cpp_copy(AGEA_copy_handler_args)
{
    AGEA_unused(dst_obj);
    AGEA_unused(src_obj);
    AGEA_unused(ooc);

    reflection::utils::as_type<T>(to) = reflection::utils::as_type<T>(from);
    return result_code::ok;
}

template <typename T>
result_code
cpp_serialize(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    reflection::utils::pack_field<T>(ptr, jc);

    return result_code::ok;
}

template <typename T>
result_code
cpp_to_string(AGEA_reflection_type_ui_args)
{
    AGEA_unused(ptr);
    auto& t = reflection::utils::as_type<T>(ptr);
    result = std::format("{}", t);

    return result_code::ok;
}

template <typename T>
result_code
cpp_deserialize(AGEA_deserialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(occ);
    AGEA_unused(jc);
    reflection::utils::extract_field<T>(ptr, jc);
    return result_code::ok;
}

template <typename T>
result_code
cpp_load_derive(AGEA_load_derive_args)
{
    AGEA_unused(ptr);
    AGEA_unused(occ);
    AGEA_unused(jc);
    reflection::utils::extract_field<T>(ptr, jc);
    return result_code::ok;
}

}  // namespace utils
}  // namespace reflection
}  // namespace agea
