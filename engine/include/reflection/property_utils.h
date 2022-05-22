#pragma once

#include "core/agea_minimal.h"
#include <functional>

#define AGEA_deseialization_args \
    blob_ptr ptr, serialization::json_conteiner &jc, model::object_constructor_context &occ
#define AGEA_deseialization_update_args \
    blob_ptr ptr, serialization::json_conteiner &jc, model::object_constructor_context &occ

#define AGEA_copy_handlfer_args                                                             \
    model::smart_object &src_obj, model::smart_object &dst_obj, blob_ptr from, blob_ptr to, \
        model::object_constructor_context &ooc

namespace Json
{
class Value;
}

namespace agea
{
namespace serialization
{
using json_conteiner = Json::Value;

}  // namespace serialization
namespace model
{
struct object_constructor_context;
}

namespace reflection
{

template <typename T>
T&
extract(blob_ptr ptr)
{
    return *(T*)(ptr);
}

template <typename T>
void
extract_field(blob_ptr ptr, serialization::json_conteiner& jc)
{
    extract<T>(ptr) = jc.as<T>();
}

inline blob_ptr
reduce_ptr(blob_ptr ptr, bool is_ptr)
{
    return is_ptr ? *(blob_ptr*)(ptr) : ptr;
}

using property_serialization_handler = std::function<bool(AGEA_deseialization_args)>;
using property_serialization_update_handler = std::function<bool(AGEA_deseialization_update_args)>;
using property_copy_handler = std::function<bool(AGEA_copy_handlfer_args)>;
}  // namespace reflection

}  // namespace agea