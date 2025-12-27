#pragma once

#include "core/model_minimal.h"

#include "core/reflection/property_utils.h"
#include "core/reflection/type_description.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace agea
{
namespace reflection
{

struct reflection_type;

class property
{
public:
    // clang-format off
    property_handler__save           save_handler            = default_save;
    property_handler__compare        compare_handler         = default_compare;
    property_handler__copy           copy_handler            = default_copy;
    property_handler__instantiate    instantiate_handler     = default_instantiate;
    property_handler__load           load_handler            = default_load;
    property_handler__to_string      to_string_handler       = default_to_string;

    // clang-format on

    blob_ptr
    get_blob(root::smart_object& obj);

private:
    static result_code
    default_compare(property_context__compare& context);

    static result_code
    default_save(property_context__save& context);

    static result_code
    default_copy(property_context__copy& context);

    static result_code
    default_instantiate(property_context__instantiate& context);

    static result_code
    default_load(property_context__load& context);

    static result_code
    default_to_string(property_context__to_string& context);

    static result_code
    deserialize_collection(reflection::property& p,
                           root::smart_object& obj,
                           const serialization::container& sc,
                           core::object_load_context& occ);

    static result_code
    load_item(reflection::property& p,
              root::smart_object& obj,
              const serialization::container& sc,
              core::object_load_context& occ);

    static result_code
    serialize_collection(const reflection::property& p,
                         const root::smart_object& obj,
                         serialization::container& sc);

    static result_code
    serialize_item(const reflection::property& p,
                   const root::smart_object& obj,
                   serialization::container& sc);

    static result_code
    compare_collection(property_context__compare& context);

    static result_code
    compare_item(property_context__compare& context);

public:
    // clang-format off
    std::string name;
    size_t offset;
    type_description type;

    std::string category;
    std::vector<std::string> hints;

    bool serializable                                               = false;
    bool has_default                                                = false;
    bool render_subobject                                           = false;

    reflection_type* rtype = nullptr;

    std::string gpu_data;
    // clang-format on
};

}  // namespace reflection
}  // namespace agea
