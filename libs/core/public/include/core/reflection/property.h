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
    property_save_handler            save_handler            = default_save;
    property_compare_handler         compare_handler         = default_compare;
    property_copy_handler            copy_handler            = default_copy;
    property_instantiate_handler     instantiate_handler     = default_instantiate;
    property_load_handler            load_handler            = default_load;
    property_to_string_handler       to_string_handler       = default_to_string;

    // clang-format on

    blob_ptr
    get_blob(root::smart_object& obj);

private:
    static result_code
    default_compare(compare_context& context);

    static result_code
    default_save(save_context& context);

    static result_code
    default_copy(copy_context& context);

    static result_code
    default_instantiate(instantiate_context& context);

    static result_code
    default_load(property_load_context& context);

    static result_code
    default_to_string(property_to_string_context& context);

    static result_code
    deserialize_collection(reflection::property& p,
                           root::smart_object& obj,
                           const serialization::conteiner& sc,
                           core::object_load_context& occ);

    static result_code
    load_item(reflection::property& p,
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
