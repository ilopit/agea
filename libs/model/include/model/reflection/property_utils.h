#pragma once

#include "model/model_minimal.h"

#include "model/model_fwds.h"
#include "serialization/serialization_fwds.h"

#include <functional>

#define AGEA_serialization_args \
    const model::smart_object &obj, ::agea::blob_ptr ptr, serialization::conteiner &jc

#define AGEA_deserialization_args                                                       \
    model::smart_object &obj, ::agea::blob_ptr ptr, const serialization::conteiner &jc, \
        model::object_constructor_context &occ

#define AGEA_deserialization_update_args \
    ::agea::blob_ptr ptr, const serialization::conteiner &jc, model::object_constructor_context &occ

#define AGEA_copy_handler_args                                                         \
    model::smart_object &src_obj, model::smart_object &dst_obj, ::agea::blob_ptr from, \
        ::agea::blob_ptr to, model::object_constructor_context &ooc

#define AGEA_protorype_handler_args                                                    \
    model::smart_object &src_obj, model::smart_object &dst_obj, ::agea::blob_ptr from, \
        ::agea::blob_ptr to, const serialization::conteiner &jc,                       \
        model::object_constructor_context &ooc

#define AGEA_compare_handler_args ::agea::blob_ptr from, ::agea::blob_ptr to

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

struct serialize_context
{
    property* p = nullptr;
    const model::smart_object* obj = nullptr;
    serialization::conteiner* sc = nullptr;
};

struct compare_context
{
    property* p = nullptr;
    const model::smart_object* src_obj = nullptr;
    const model::smart_object* dst_obj = nullptr;
};

struct copy_context
{
    property* src_property = nullptr;
    property* dst_property = nullptr;
    model::smart_object* src_obj = nullptr;
    model::smart_object* dst_obj = nullptr;
    model::object_constructor_context* occ = nullptr;
};

struct property_prototype_context
{
    property* src_property = nullptr;
    property* dst_property = nullptr;
    model::smart_object* src_obj = nullptr;
    model::smart_object* dst_obj = nullptr;
    model::object_constructor_context* occ = nullptr;
    const serialization::conteiner* sc = nullptr;
};

template <typename T>
T&
extract(blob_ptr ptr)
{
    return *(T*)(ptr);
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

template <typename T>
result_code
fast_compare(blob_ptr from, blob_ptr to)
{
    return memcmp(to, from, sizeof(T)) ? result_code::failed : result_code::ok;
}

template <typename T>
result_code
full_compare(blob_ptr from, blob_ptr to)
{
    return (extract<T>(to) == extract<T>(from)) ? result_code::ok : result_code::failed;
}

// clang-format off

using property_deserialization_handler  = std::function<result_code(deserialize_context&)>;
using property_serialization_handler    = std::function<result_code(serialize_context&)>;
using property_compare_handler          = std::function<result_code(compare_context&)>;
using property_copy_handler             = std::function<result_code(copy_context&)>;
using property_prototype_handler        = std::function<result_code(property_prototype_context&)>;

using type_serialization_handler        = std::function<result_code(AGEA_serialization_args)>;
using type_deserialization_handler      = std::function<result_code(AGEA_deserialization_args)>;
using type_serialization_update_handler = std::function<result_code(AGEA_deserialization_update_args)>;
using type_copy_handler                 = std::function<result_code(AGEA_copy_handler_args)>;
using type_compare_handler              = std::function<result_code(AGEA_compare_handler_args)>;
using type_read_from_handler            = std::function<result_code(AGEA_read_from_property_args)>;
// clang-format on

}  // namespace reflection

}  // namespace agea