#pragma once

#include "core/model_minimal.h"
#include "core/reflection/property.h"
#include "core/architype.h"
#include "core/model_fwds.h"

#include <serialization/serialization_fwds.h>

#include <utils/id.h>

#include <vector>

#define AGEA_serialization_args                                  \
    const ::agea::root::smart_object &obj, ::agea::blob_ptr ptr, \
        ::agea::serialization::conteiner &jc

#define AGEA_deserialization_args                          \
    ::agea::root::smart_object &obj, ::agea::blob_ptr ptr, \
        const ::agea::serialization::conteiner &jc, ::agea::core::object_load_context &occ

#define AGEA_deserialization_update_args                              \
    ::agea::blob_ptr ptr, const ::agea::serialization::conteiner &jc, \
        ::agea::core::object_load_context &occ

#define AGEA_copy_handler_args                                                \
    ::agea::root::smart_object &src_obj, ::agea::root::smart_object &dst_obj, \
        ::agea::blob_ptr from, ::agea::blob_ptr to, ::agea::core::object_load_context &ooc

#define AGEA_protorype_handler_args                                                             \
    ::agea::root::smart_object &src_obj, ::agea::root::smart_object &dst_obj,                   \
        ::agea::blob_ptr from, ::agea::blob_ptr to, const ::agea::serialization::conteiner &jc, \
        ::agea::core::object_load_context &ooc

#define AGEA_compare_handler_args ::agea::blob_ptr from, ::agea::blob_ptr to

#define AGEA_reflection_type_ui_args ::agea::blob_ptr ptr

#define AGEA_AR_render_ctor_args agea::render_bridge &rb, root::smart_object &, bool

#define AGEA_AR_render_dtor_args agea::render_bridge &rb, root::smart_object &, bool

#define AGEA_AR_alloc_args const agea::utils::id& id

namespace agea
{

class render_bridge;

namespace reflection
{

using type_serialization_handler = result_code (*)(AGEA_serialization_args);
using type_deserialization_handler = result_code (*)(AGEA_deserialization_args);
using type_deserialisation_with_prototype_handler =
    result_code (*)(AGEA_deserialization_update_args);
using type_copy_handler = result_code (*)(AGEA_copy_handler_args);
using type_compare_handler = result_code (*)(AGEA_compare_handler_args);
using type_ui_handler = result_code (*)(AGEA_reflection_type_ui_args);
using type_rendler_ctor = result_code (*)(AGEA_AR_render_ctor_args);
using type_rendler_dtor = result_code (*)(AGEA_AR_render_dtor_args);
using type_allocator_handler = std::shared_ptr<root::smart_object> (*)(AGEA_AR_alloc_args);

using property_list = std::vector<std::shared_ptr<property>>;

struct reflection_type
{
    void
    finalize_handlers();

    void
    update()
    {
        if (arch != core::architype::unknown)
        {
            return;
        }

        if (parent)
        {
            parent->update();

            arch = parent->arch;
        }
    }

    int type_id = -1;
    agea::utils::id module_id;
    agea::utils::id type_name;
    uint32_t size = 0;
    core::architype arch = core::architype::unknown;

    reflection_type* parent = nullptr;

    std::unordered_map<std::string, property_list> m_editor_properties;
    property_list m_properties;
    property_list m_serilalization_properties;

    // clang-format off
    type_serialization_handler                  serialization = nullptr;
    type_deserialization_handler                deserialization = nullptr;
    type_deserialisation_with_prototype_handler deserialization_with_proto = nullptr;
    type_copy_handler                           copy = nullptr;
    type_compare_handler                        compare = nullptr;
    type_ui_handler                             ui = nullptr;
    type_rendler_ctor                           render_ctor = nullptr;
    type_rendler_dtor                           render_dtor = nullptr;
    type_allocator_handler                      alloc = nullptr;

    // clang-format on

    bool initialized = false;
};

class reflection_type_registry
{
public:
    void
    add_type(reflection_type* t);

    reflection_type*
    get_type(const int id);

    reflection_type*
    get_type(const agea::utils::id& id);

    void
    finilaze();

private:
    std::unordered_map<int, reflection_type*> m_types;
    std::unordered_map<agea::utils::id, reflection_type*> m_types_by_name;
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