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
struct type_context__save
{
    const root::smart_object* owner_obj = nullptr;
    blob_ptr obj = nullptr;
    serialization::container* jc = nullptr;
};

struct type_context__load
{
    root::smart_object* owner_obj = nullptr;
    blob_ptr obj = nullptr;
    const serialization::container* jc = nullptr;
    core::object_load_context* occ = nullptr;
};

struct type_context__copy
{
    root::smart_object* dst_owner_obj = nullptr;
    blob_ptr src_obj = nullptr;

    root::smart_object* src_owner_obj = nullptr;
    blob_ptr dst_obj = nullptr;

    core::object_load_context* occ = nullptr;
};

struct type_context__compare
{
    blob_ptr left_obj = nullptr;
    blob_ptr right_obj = nullptr;
};

struct type_context__to_string
{
    root::smart_object* owner_obj = nullptr;
    blob_ptr obj = nullptr;
    std::string* result = nullptr;
};

struct type_context__render
{
    render_bridge* rb = nullptr;
    root::smart_object* obj = nullptr;
    bool flag = false;
};

struct type_context__alloc
{
    const utils::id* id = nullptr;
};

using type_handler__save = result_code (*)(type_context__save&);
using type_handler__load = result_code (*)(type_context__load&);
using type_handler__copy = result_code (*)(type_context__copy&);
using type_handler__instantiate = result_code (*)(type_context__copy&);
using type_handler__compare = result_code (*)(type_context__compare&);
using type_handler__to_string = result_code (*)(type_context__to_string&);
using type_handler__render_ctor = result_code (*)(type_context__render&);
using type_handler__render_dtor = result_code (*)(type_context__render&);
using type_handler__alloc = std::shared_ptr<root::smart_object> (*)(type_context__alloc&);
using type_handler__cparams_alloc = std::unique_ptr<::agea::root::base_construct_params> (*)();

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
    type_handler__alloc                         alloc = nullptr;
    type_handler__cparams_alloc                 cparams_alloc = nullptr;

    type_handler__save                          save = nullptr;
    type_handler__load                          load = nullptr;
    type_handler__copy                          copy = nullptr;
    type_handler__instantiate                   instantiate = nullptr;
    type_handler__compare                       compare = nullptr;
    type_handler__to_string                     to_string = nullptr;

    type_handler__render_ctor                   render_constructor = nullptr;
    type_handler__render_dtor                   render_destructor = nullptr;

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