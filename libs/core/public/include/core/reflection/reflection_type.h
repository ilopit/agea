#pragma once

#include "core/reflection/property.h"
#include "core/reflection/function.h"
#include "core/architype.h"

#include <serialization/serialization_fwds.h>

#include <utils/id.h>

#include <vector>

namespace agea
{

class render_bridge;

namespace root
{
struct base_construct_params;
}

namespace reflection
{

// Type-level context structures for reflection handlers
struct type_save_context
{
    const root::smart_object* obj = nullptr;
    blob_ptr ptr = nullptr;
    serialization::conteiner* jc = nullptr;
};

struct type_load_context
{
    root::smart_object* obj = nullptr;
    blob_ptr ptr = nullptr;
    const serialization::conteiner* jc = nullptr;
    core::object_load_context* occ = nullptr;
};

struct type_copy_context
{
    root::smart_object* src_obj = nullptr;
    root::smart_object* dst_obj = nullptr;
    blob_ptr from = nullptr;
    blob_ptr to = nullptr;
    core::object_load_context* occ = nullptr;
};

struct type_compare_context
{
    blob_ptr from = nullptr;
    blob_ptr to = nullptr;
};

struct type_ui_context
{
    blob_ptr ptr = nullptr;
    std::string* result = nullptr;
};

struct type_render_context
{
    render_bridge* rb = nullptr;
    root::smart_object* obj = nullptr;
    bool flag = false;
};

struct type_alloc_context
{
    const utils::id* id = nullptr;
};

using type_save_handler = result_code (*)(type_save_context&);
using type_load_handler = result_code (*)(type_load_context&);
using type_copy_handler = result_code (*)(type_copy_context&);
using type_instantiate_handler = result_code (*)(type_copy_context&);
using type_compare_handler = result_code (*)(type_compare_context&);
using type_ui_handler = result_code (*)(type_ui_context&);
using type_render_ctor = result_code (*)(type_render_context&);
using type_render_dtor = result_code (*)(type_render_context&);
using type_allocator_handler = std::shared_ptr<root::smart_object> (*)(type_alloc_context&);
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

    type_save_handler                           save = nullptr;
    type_load_handler                           load = nullptr;
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