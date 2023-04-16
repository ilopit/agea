#pragma once

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
default_copy(blob_ptr from, blob_ptr to)
{
    as_type<T>(to) = as_type<T>(from);
}

template <typename T>
void
fast_copy(blob_ptr from, blob_ptr to)
{
    memcpy(to, from, sizeof(T));
}

template <typename T>
result_code
fast_compare(blob_ptr from, blob_ptr to)
{
    return memcmp(to, from, sizeof(T)) ? result_code::failed : result_code::ok;
}

template <typename T>
result_code
default_compare(blob_ptr from, blob_ptr to)
{
    return (as_type<T>(to) == as_type<T>(from)) ? result_code::ok : result_code::failed;
}

}  // namespace utils
}  // namespace reflection
}  // namespace agea
