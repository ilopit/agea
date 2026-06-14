#pragma once

#include "core/reflection/property.h"
#include "core/reflection/function.h"
#include "core/architype.h"

#include <serialization/serialization_fwds.h>

#include <utils/id.h>

#include <vector>

// Forward decl so reflection_type can carry json_save/json_load function
// pointers without dragging jsoncpp into core's compile graph. Implementations
// live in the editor RPC path (libs/core/private/src/reflection/json_*.cpp)
// where jsoncpp is actually pulled in.
namespace Json
{
class Value;
}

namespace kryga
{

class render_translator;

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
    core::object_constructor* ctor = nullptr;
};

struct type_context__copy
{
    root::smart_object* dst_owner_obj = nullptr;
    blob_ptr src_obj = nullptr;

    root::smart_object* src_owner_obj = nullptr;
    blob_ptr dst_obj = nullptr;

    core::object_constructor* ctor = nullptr;
};

struct type_context__compare
{
    blob_ptr left_obj = nullptr;
    blob_ptr right_obj = nullptr;
};

struct type_context__render_cmd_build
{
    render_translator* rb = nullptr;
    root::smart_object* obj = nullptr;
    bool flag = false;
};

struct type_context__alloc
{
    const utils::id* id = nullptr;
};

// JSON-native variants used by the editor RPC. Engine YAML save/load is the
// canonical content path; these are the wire-format twins so RPC doesn't pay
// a YAML→JSON conversion per call. Types opt in by registering both.
struct type_context__json_save
{
    const root::smart_object* owner_obj = nullptr;
    blob_ptr obj = nullptr;
    Json::Value* jc = nullptr;
};

struct type_context__json_load
{
    root::smart_object* owner_obj = nullptr;
    blob_ptr obj = nullptr;
    const Json::Value* jc = nullptr;
};

using type_handler__save = result_code (*)(type_context__save&);
using type_handler__load = result_code (*)(type_context__load&);
using type_handler__copy = result_code (*)(type_context__copy&);
using type_handler__instantiate = result_code (*)(type_context__copy&);
using type_handler__compare = result_code (*)(type_context__compare&);
using type_handler__render_cmd_builder = result_code (*)(type_context__render_cmd_build&);
using type_handler__render_cmd_destroyer = result_code (*)(type_context__render_cmd_build&);
using type_handler__render_cmd_transform = result_code (*)(type_context__render_cmd_build&);
using type_handler__gpu_pack = void (*)(const void* src, void* dst);

struct gpu_texture_slot_ref
{
    uint32_t slot;
    const void* data;
};
using type_handler__gpu_texture_collect = uint32_t (*)(const void* src, gpu_texture_slot_ref* out);

using type_handler__alloc = std::shared_ptr<root::smart_object> (*)(type_context__alloc&);
using type_handler__cparams_alloc = std::unique_ptr<::kryga::root::base_construct_params> (*)();
using type_handler__cparams_json_load = result_code (*)(::kryga::root::base_construct_params*,
                                                        const Json::Value&);
using type_handler__json_save = result_code (*)(type_context__json_save&);
using type_handler__json_load = result_code (*)(type_context__json_load&);

using property_list = std::vector<std::shared_ptr<property>>;
using function_list = std::vector<std::shared_ptr<function>>;

struct reflection_type
{
    enum class reflection_type_class
    {
        kryga_unknown = 0,
        kryga_class,
        kryga_struct,
        kryga_external
    };

    reflection_type(int type_id, const kryga::utils::id& type_name);
    ~reflection_type();

    void
    override();

    std::string
    as_string() const;

    function*
    find_function(const std::string& name) const;

    int type_id = -1;
    kryga::utils::id module_id;
    kryga::utils::id type_name;
    uint32_t size = 0;

    core::architype arch = core::architype::unknown;
    reflection_type_class type_class = reflection_type_class::kryga_unknown;

    reflection_type* parent = nullptr;

    std::unordered_map<std::string, property_list> m_editor_properties;
    property_list m_properties;
    function_list m_functions;
    property_list m_serialization_properties;

    // clang-format off
    type_handler__alloc                         alloc = nullptr;
    type_handler__cparams_alloc                 cparams_alloc = nullptr;
    type_handler__cparams_json_load             cparams_json_load = nullptr;

    type_handler__save                          save = nullptr;
    type_handler__load                          load = nullptr;
    type_handler__copy                          copy = nullptr;
    type_handler__instantiate                   instantiate = nullptr;
    type_handler__compare                       compare = nullptr;

    // Editor RPC wire format. Optional — if either is null the RPC layer
    // logs and skips the field. Defaults registered for primitives + vec*
    // by reflection::register_default_json_handlers(); types in new packages
    // can register their own.
    type_handler__json_save                     json_save = nullptr;
    type_handler__json_load                     json_load = nullptr;

    type_handler__render_cmd_builder            render_cmd_builder = nullptr;
    type_handler__render_cmd_destroyer          render_cmd_destroyer = nullptr;
    type_handler__render_cmd_transform          render_cmd_transform = nullptr;

    type_handler__gpu_pack                      gpu_pack = nullptr;
    size_t                                      gpu_data_size = 0;

    type_handler__gpu_texture_collect           gpu_texture_collect = nullptr;
    uint32_t                                    gpu_texture_slot_count = 0;

    // clang-format on

    std::string mcp_schema;
    std::string mcp_hint;
    std::string source_file;

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
    get_type(const kryga::utils::id& id);

    void
    unload_type(const int type_id, const kryga::utils::id& id);

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
    std::unordered_map<kryga::utils::id, reflection_type*> m_types_by_name;
};

}  // namespace reflection

}  // namespace kryga