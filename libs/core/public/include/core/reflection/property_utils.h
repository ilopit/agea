#pragma once

#include "core/model_minimal.h"

#include "core/model_fwds.h"
#include "serialization/serialization_fwds.h"

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
    root::smart_object* obj = nullptr;
    const serialization::conteiner* sc = nullptr;
    core::object_load_context* occ = nullptr;
};

struct serialize_context
{
    property* p = nullptr;
    const root::smart_object* obj = nullptr;
    serialization::conteiner* sc = nullptr;
};

struct compare_context
{
    property* p = nullptr;
    const root::smart_object* src_obj = nullptr;
    const root::smart_object* dst_obj = nullptr;
};

struct copy_context
{
    property* src_property = nullptr;
    property* dst_property = nullptr;
    root::smart_object* src_obj = nullptr;
    root::smart_object* dst_obj = nullptr;
    core::object_load_context* occ = nullptr;
};

struct property_prototype_context
{
    property* src_property = nullptr;
    property* dst_property = nullptr;
    root::smart_object* src_obj = nullptr;
    root::smart_object* dst_obj = nullptr;
    core::object_load_context* occ = nullptr;
    const serialization::conteiner* sc = nullptr;
};

// clang-format off

using property_deserialization_handler  = result_code(*)(deserialize_context&);
using property_serialization_handler    = result_code(*)(serialize_context&);
using property_compare_handler          = result_code(*)(compare_context&);
using property_copy_handler             = result_code(*)(copy_context&);
using property_prototype_handler        = result_code(*)(property_prototype_context&);

// clang-format on

}  // namespace reflection

}  // namespace agea