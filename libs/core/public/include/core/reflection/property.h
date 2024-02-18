#pragma once

#include "core/model_minimal.h"

#include "core/reflection/reflection_type_utils.h"
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
    property_deserialization_handler deserialization_handler = default_deserialize;
    property_serialization_handler   serialization_handler   = default_serialize;
    property_compare_handler         compare_handler         = default_compare;
    property_copy_handler            copy_handler            = default_copy;
    property_prototype_handler       protorype_handler       = default_prototype;
    property_to_string_handler       to_string_handler       = default_to_string;



    blob_ptr get_blob(root::smart_object& obj);

    // clang-format on

    static result_code
    deserialize_update(reflection::property& p,
                       blob_ptr ptr,
                       const serialization::conteiner& sc,
                       core::object_load_context& occ);

private:
    static result_code
    default_compare(compare_context& context);

    static result_code
    default_deserialize(deserialize_context& context);

    static result_code
    default_serialize(serialize_context& context);

    static result_code
    default_copy(copy_context& context);

    static result_code
    default_prototype(property_prototype_context& context);

    static result_code
    default_to_string(property_to_string_context& context);

    static result_code
    deserialize_collection(reflection::property& p,
                           root::smart_object& obj,
                           const serialization::conteiner& sc,
                           core::object_load_context& occ);

    static result_code
    deserialize_item(reflection::property& p,
                     root::smart_object& obj,
                     const serialization::conteiner& sc,
                     core::object_load_context& occ);

    static result_code
    serialize_collection(const reflection::property& p,
                         const root::smart_object& obj,
                         serialization::conteiner& sc);

    static result_code
    serialize_item(const reflection::property& p,
                   const root::smart_object& obj,
                   serialization::conteiner& sc);

    static result_code
    deserialize_update_collection(reflection::property& p,
                                  blob_ptr ptr,
                                  const serialization::conteiner& sc,
                                  core::object_load_context& occ);

    static result_code
    deserialize_update_item(reflection::property& p,
                            blob_ptr ptr,
                            const serialization::conteiner& sc,
                            core::object_load_context& occ);

    static result_code
    compare_collection(compare_context& context);

    static result_code
    compare_item(compare_context& context);

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
