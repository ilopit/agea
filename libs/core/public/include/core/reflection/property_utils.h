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

struct property_context__save
{
    property* p = nullptr;
    const root::smart_object* obj = nullptr;
    serialization::container* sc = nullptr;
};

struct property_context__compare
{
    property* p = nullptr;
    const root::smart_object* src_obj = nullptr;
    const root::smart_object* dst_obj = nullptr;
};

struct property_context__copy
{
    property* src_property = nullptr;
    property* dst_property = nullptr;
    root::smart_object* src_obj = nullptr;
    root::smart_object* dst_obj = nullptr;
    core::object_load_context* occ = nullptr;
};

struct property_context__instantiate
{
    property* src_property = nullptr;
    property* dst_property = nullptr;
    root::smart_object* src_obj = nullptr;
    root::smart_object* dst_obj = nullptr;
    core::object_load_context* occ = nullptr;
};

struct property_context__prototype
{
    property* src_property = nullptr;
    property* dst_property = nullptr;
    root::smart_object* src_obj = nullptr;
    root::smart_object* dst_obj = nullptr;
    core::object_load_context* occ = nullptr;
    const serialization::container* sc = nullptr;
};

struct property_context__load
{
    property* src_property = nullptr;
    property* dst_property = nullptr;
    root::smart_object* src_obj = nullptr;
    root::smart_object* dst_obj = nullptr;
    core::object_load_context* occ = nullptr;
    const serialization::container* sc = nullptr;
};

struct property_context__to_string
{
    property* prop = nullptr;
    root::smart_object* obj = nullptr;
    std::string result;
};

// clang-format off

using property_handler__save            = result_code(*)(property_context__save&);
using property_handler__compare         = result_code(*)(property_context__compare&);
using property_handler__copy            = result_code(*)(property_context__copy&);
using property_handler__instantiate     = result_code(*)(property_context__instantiate&);
using property_handler__load            = result_code(*)(property_context__load&);
using property_handler__to_string       = result_code(*)(property_context__to_string&);

// clang-format on

}  // namespace reflection

}  // namespace agea