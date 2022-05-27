#pragma once

#include "core/agea_minimal.h"

#include "model/model_fwds.h"
#include "serialization/serialization_fwds.h"

#include <functional>

#define AGEA_serialization_args ::agea::blob_ptr ptr, serialization::conteiner &jc

#define AGEA_deserialization_args \
    ::agea::blob_ptr ptr, const serialization::conteiner &jc, model::object_constructor_context &occ

#define AGEA_deserialization_update_args \
    ::agea::blob_ptr ptr, const serialization::conteiner &jc, model::object_constructor_context &occ

#define AGEA_copy_handler_args                                                         \
    model::smart_object &src_obj, model::smart_object &dst_obj, ::agea::blob_ptr from, \
        ::agea::blob_ptr to, model::object_constructor_context &ooc

#define AGEA_read_from_property_args ::agea::blob_ptr ptr, fixed_size_buffer &buf

namespace YAML
{
class Node;
}

namespace agea
{

namespace reflection
{

class property;

struct deserialize_context
{
    property* p = nullptr;
    model::smart_object* obj = nullptr;
    const serialization::conteiner* sc = nullptr;
    model::object_constructor_context* occ = nullptr;
};

template <typename T>
T&
extract(blob_ptr ptr)
{
    return *(T*)(ptr);
}

template <typename T>
void
extract_field(blob_ptr ptr, const serialization::conteiner& jc)
{
    extract<T>(ptr) = jc.as<T>();
}

template <typename T>
void
pack_field(blob_ptr ptr, serialization::conteiner& jc)
{
    jc = extract<T>(ptr);
}

inline blob_ptr
reduce_ptr(blob_ptr ptr, bool is_ptr)
{
    return is_ptr ? *(blob_ptr*)(ptr) : ptr;
}

template <typename T>
void
full_copy(blob_ptr from, blob_ptr to)
{
    extract<T>(to) = extract<T>(from);
}

template <typename T>
void
fast_copy(blob_ptr from, blob_ptr to)
{
    memcpy(to, from, sizeof(T));
}

// clang-format off

using property_deserialization_handler  = std::function<bool(deserialize_context&)>;

using type_serialization_handler        = std::function<bool(AGEA_serialization_args)>;
using type_deserialization_handler      = std::function<bool(AGEA_deserialization_args)>;
using type_serialization_update_handler = std::function<bool(AGEA_deserialization_update_args)>;
using type_copy_handler                 = std::function<bool(AGEA_copy_handler_args)>;
using type_read_from_handler            = std::function<bool(AGEA_read_from_property_args)>;
// clang-format on
}  // namespace reflection

}  // namespace agea