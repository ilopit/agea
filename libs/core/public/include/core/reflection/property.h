#pragma once

#include "core/model_minimal.h"

#include "core/reflection/property_utils.h"
#include "core/reflection/type_description.h"

#ifdef KRG_ENFORCE_READONLY
#include <packages/root/model/smart_object.h>
#include <utils/check.h>
#endif

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace kryga
{
namespace reflection
{

struct reflection_type;

enum class instantiate_mode : uint8_t
{
    instantiate,
    share,
};

#ifdef KRG_ENFORCE_READONLY

template <typename Fn>
struct guarded_handler
{
    Fn fn = nullptr;

    guarded_handler() = default;
    guarded_handler(Fn f)
        : fn(f)
    {
    }
    guarded_handler&
    operator=(Fn f)
    {
        fn = f;
        return *this;
    }

    template <typename Ctx>
    result_code
    operator()(Ctx& ctx) const
    {
        root::smart_object* target;
        if constexpr (requires { ctx.dst_obj; })
            target = ctx.dst_obj;
        else
            target = ctx.obj;
        KRG_check(!target->get_flags().readonly, "writing to readonly object");
        return fn(ctx);
    }
};

using property_handler__copy_guarded        = guarded_handler<property_handler__copy>;
using property_handler__instantiate_guarded = guarded_handler<property_handler__instantiate>;
using property_handler__load_guarded        = guarded_handler<property_handler__load>;
using property_handler__json_set_guarded    = guarded_handler<property_handler__json_set>;

#else

using property_handler__copy_guarded        = property_handler__copy;
using property_handler__instantiate_guarded = property_handler__instantiate;
using property_handler__load_guarded        = property_handler__load;
using property_handler__json_set_guarded    = property_handler__json_set;

#endif

class property
{
public:
    // clang-format off
    property_handler__save               save_handler        = default_save;
    property_handler__compare            compare_handler     = default_compare;
    property_handler__copy_guarded       copy_handler        = default_copy;
    property_handler__instantiate_guarded instantiate_handler = default_instantiate;
    property_handler__load_guarded       load_handler        = default_load;
    property_handler__json_get           json_get            = default_json_get;
    property_handler__json_set_guarded   json_set            = default_json_set;

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
    default_json_get(property_context__json_get& context);

    static result_code
    default_json_set(property_context__json_set& context);

    static result_code
    deserialize_collection(reflection::property& p,
                           root::smart_object& obj,
                           const serialization::container& sc,
                           core::object_constructor* ctor);

    static result_code
    load_item(reflection::property& p,
              root::smart_object& obj,
              const serialization::container& sc,
              core::object_constructor* ctor);

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
    instantiate_mode inst_mode                                      = instantiate_mode::instantiate;

    reflection_type* rtype = nullptr;

    std::string gpu_data;
    int32_t gpu_texture_slot                                       = -1;
    std::string mcp_hint;
    // clang-format on
};

}  // namespace reflection
}  // namespace kryga
