#pragma once

#include "core/reflection/property.h"
#include "core/reflection/function.h"
#include "core/architype.h"

#include <serialization/serialization_fwds.h>

#include <utils/id.h>

#include <vector>

#define AGEA_serialization_args                                  \
    const ::agea::root::smart_object &obj, ::agea::blob_ptr ptr, \
        ::agea::serialization::conteiner &jc

#define AGEA_deserialization_args                          \
    ::agea::root::smart_object &obj, ::agea::blob_ptr ptr, \
        const ::agea::serialization::conteiner &jc, ::agea::core::object_load_context &occ

#define AGEA_load_derive_args                              \
    ::agea::root::smart_object &obj, ::agea::blob_ptr ptr, \
        const ::agea::serialization::conteiner &jc, ::agea::core::object_load_context &occ

#define AGEA_deserialization_update_args                              \
    ::agea::blob_ptr ptr, const ::agea::serialization::conteiner &jc, \
        ::agea::core::object_load_context &occ

#define AGEA_copy_handler_args                                                \
    ::agea::root::smart_object &src_obj, ::agea::root::smart_object &dst_obj, \
        ::agea::blob_ptr from, ::agea::blob_ptr to, ::agea::core::object_load_context &ooc

#define AGEA_instantiate_handler_args AGEA_copy_handler_args

#define AGEA_protorype_handler_args                                                             \
    ::agea::root::smart_object &src_obj, ::agea::root::smart_object &dst_obj,                   \
        ::agea::blob_ptr from, ::agea::blob_ptr to, const ::agea::serialization::conteiner &jc, \
        ::agea::core::object_load_context &ooc

#define AGEA_compare_handler_args ::agea::blob_ptr from, ::agea::blob_ptr to

#define AGEA_reflection_type_ui_args ::agea::blob_ptr ptr, std::string &result

#define AGEA_AR_render_ctor_args agea::render_bridge &rb, root::smart_object &, bool

#define AGEA_AR_render_dtor_args agea::render_bridge &rb, root::smart_object &, bool

#define AGEA_AR_alloc_args const agea::utils::id& id

namespace agea
{

class render_bridge;

namespace root
{
struct base_construct_params;
}

namespace reflection
{

using type_serialization_handler = result_code (*)(AGEA_serialization_args);
using type_deserialization_handler = result_code (*)(AGEA_deserialization_args);
using type_load_derive_handler = result_code (*)(AGEA_load_derive_args);
using type_deserialisation_with_prototype_handler =
    result_code (*)(AGEA_deserialization_update_args);
using type_copy_handler = result_code (*)(AGEA_copy_handler_args);
using type_instantiate_handler = result_code (*)(AGEA_instantiate_handler_args);
using type_compare_handler = result_code (*)(AGEA_compare_handler_args);
using type_ui_handler = result_code (*)(AGEA_reflection_type_ui_args);
using type_render_ctor = result_code (*)(AGEA_AR_render_ctor_args);
using type_render_dtor = result_code (*)(AGEA_AR_render_dtor_args);
using type_allocator_handler = std::shared_ptr<root::smart_object> (*)(AGEA_AR_alloc_args);
using type_default_construction_params_handler =
    std::unique_ptr<::agea::root::base_construct_params> (*)();

using property_list = std::vector<std::shared_ptr<property>>;
using function_list = std::vector<std::shared_ptr<function>>;

struct reflection_type
{
    enum class reflection_type_class
    {
        agea_unknown = 0,
        agea_class,
        agea_struct,
        agea_external
    };

    reflection_type(int type_id, const agea::utils::id& type_name);
    ~reflection_type();

    void
    override();

    std::string
    as_string() const;

    int type_id = -1;
    agea::utils::id module_id;
    agea::utils::id type_name;
    uint32_t size = 0;

    core::architype arch = core::architype::unknown;
    reflection_type_class type_class = reflection_type_class::agea_unknown;

    reflection_type* parent = nullptr;

    std::unordered_map<std::string, property_list> m_editor_properties;
    property_list m_properties;
    function_list m_functions;
    property_list m_serilalization_properties;

    // clang-format off
    type_allocator_handler                      alloc = nullptr;
    type_default_construction_params_handler    cparams_alloc = nullptr;

    type_serialization_handler                  serialize = nullptr;
    type_deserialization_handler                deserialize = nullptr;
    type_deserialisation_with_prototype_handler deserialize_with_proto = nullptr;
    type_load_derive_handler                    load_derive = nullptr;
    type_copy_handler                           copy = nullptr;
    type_instantiate_handler                    instantiate = nullptr;
    type_compare_handler                        compare = nullptr;
    type_ui_handler                             to_string = nullptr;

    type_render_ctor                            render_constructor = nullptr;
    type_render_dtor                            render_destructor = nullptr;

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
    unload_type(const int type_id, const agea::utils::id& id);

    const auto&
    get_types_to_id() const
    {
        return m_types;
    }

    const auto&
    get_types_to_name() const
    {
        return m_types_by_name;
    }

private:
    std::unordered_map<int, reflection_type*> m_types;
    std::unordered_map<agea::utils::id, reflection_type*> m_types_by_name;
};

}  // namespace reflection

}  // namespace agea