#pragma once

#include <utils/id.h>

#include "model/model_minimal.h"
#include "model/reflection/property.h"

#include "model/model_fwds.h"
#include "serialization/serialization_fwds.h"

#include <functional>

#include <vector>

#define AGEA_serialization_args                                   \
    const ::agea::model::smart_object &obj, ::agea::blob_ptr ptr, \
        ::agea::serialization::conteiner &jc

#define AGEA_deserialization_args                           \
    ::agea::model::smart_object &obj, ::agea::blob_ptr ptr, \
        const ::agea::serialization::conteiner &jc, ::agea::model::object_load_context &occ

#define AGEA_deserialization_update_args                              \
    ::agea::blob_ptr ptr, const ::agea::serialization::conteiner &jc, \
        ::agea::model::object_load_context &occ

#define AGEA_copy_handler_args                                                  \
    ::agea::model::smart_object &src_obj, ::agea::model::smart_object &dst_obj, \
        ::agea::blob_ptr from, ::agea::blob_ptr to, ::agea::model::object_load_context &ooc

#define AGEA_protorype_handler_args                                                             \
    ::agea::model::smart_object &src_obj, ::agea::model::smart_object &dst_obj,                 \
        ::agea::blob_ptr from, ::agea::blob_ptr to, const ::agea::serialization::conteiner &jc, \
        ::agea::model::object_load_context &ooc

#define AGEA_compare_handler_args ::agea::blob_ptr from, ::agea::blob_ptr to

#define AGEA_reflection_type_ui_args ::agea::blob_ptr ptr

namespace agea
{
namespace reflection
{

using type_serialization_handler = result_code (*)(AGEA_serialization_args);
using type_deserialization_handler = result_code (*)(AGEA_deserialization_args);
using type_deserialisation_with_prototype_handler =
    result_code (*)(AGEA_deserialization_update_args);
using type_copy_handler = result_code (*)(AGEA_copy_handler_args);
using type_compare_handler = result_code (*)(AGEA_compare_handler_args);
using type_ui_handler = result_code (*)(AGEA_reflection_type_ui_args);

using property_list = std::vector<std::shared_ptr<property>>;

struct reflection_type
{
    int type_id = -1;
    agea::utils::id module_id;
    agea::utils::id type_name;
    uint32_t size = 0;

    reflection_type* parent = nullptr;

    std::unordered_map<std::string, property_list> m_editor_properties;
    property_list m_properties;
    property_list m_serilalization_properties;

    type_serialization_handler serialization = nullptr;
    type_deserialization_handler deserialization = nullptr;
    type_deserialisation_with_prototype_handler deserialization_with_proto = nullptr;
    type_copy_handler copy = nullptr;
    type_compare_handler compare = nullptr;
    type_ui_handler ui = nullptr;

    bool initialized = false;
};

class reflection_type_registry
{
public:
    void
    add_type(reflection_type&& t)
    {
        m_types[t.type_id] = std::move(t);
    }

    reflection_type*
    get_type(const int& id)
    {
        auto itr = m_types.find(id);

        return itr != m_types.end() ? &itr->second : nullptr;
    }

    void
    finilaze();

private:
    std::unordered_map<int, reflection_type> m_types;
};

}  // namespace reflection

namespace glob
{
struct reflection_type_registry
    : public ::agea::singleton_instance<::agea::reflection::reflection_type_registry,
                                        reflection_type_registry>
{
};

}  // namespace glob
}  // namespace agea