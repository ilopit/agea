#pragma once

#include "packages/root/model/smart_object.ar.h"

#include <core/architype.h>
#include <core/reflection/types.h>
#include <ar/ar_defines.h>

#include <utils/id.h>
#include <utils/check.h>
#include <utils/defines_utils.h>

#include <optional>
#include <string>
#include <memory>
#include <bitset>

#define KRG_gen_class_meta_super(t) \
    using this_class = t;           \
    using base_class = t;           \
    t();                            \
    virtual ~t();                   \
    t(t&) = delete;                 \
    t& operator=(t&) = delete;

#define KRG_gen_class_meta(t, b)      \
    using this_class = t;             \
    using base_class = b::this_class; \
    t();                              \
    ~t();                             \
    t(t&) = delete;                   \
    t& operator=(t&) = delete;

#define KRG_gen_construct_params struct construct_params : base_class::construct_params

#define KRG_gen_meta_api                                                                       \
                                                                                               \
    static ::kryga::utils::id AR_TYPE_id();                                                    \
                                                                                               \
    static const ::kryga::reflection::reflection_type& AR_TYPE_reflection();                   \
                                                                                               \
    virtual bool META_construct(const ::kryga::root::base_construct_params& i);                \
                                                                                               \
    virtual bool META_default_construct(const ::kryga::root::base_construct_params& i);        \
                                                                                               \
    static std::shared_ptr<this_class> AR_TYPE_create_empty_obj(const ::kryga::utils::id& id); \
                                                                                               \
    static std::shared_ptr<::kryga::root::smart_object> AR_TYPE_create_empty_gen_obj(          \
        ::kryga::reflection::type_context__alloc& ctx);                                        \
                                                                                               \
    static std::unique_ptr<::kryga::root::base_construct_params>                               \
    AR_TYPE_create_gen_default_cparams();

namespace kryga
{

namespace reflection
{
struct reflection_type;
struct type_context__alloc;
}  // namespace reflection

namespace core
{
class object_constructor;
class package;
class level;

}  // namespace core

namespace root
{

template <typename T>
void
extract(const std::optional<T>& o, T& v)
{
    if (o.has_value())
    {
        v = *o;
    }
}

class smart_object;

struct base_construct_params
{
};

enum class smart_object_state
{
    empty = 0,
    loaded,
    constructed,
    render_preparing,
    render_ready
};

struct smart_object_flags
{
    bool instance_obj : 1 = false;
    bool derived_obj : 1 = false;
    bool runtime_obj : 1 = false;
    bool mirror_obj : 1 = false;
    bool default_obj : 1 = false;
};

using smart_object_ptr = std::shared_ptr<smart_object>;
// clang-format off
KRG_ar_class(architype                      = smart_object,
              copy_handler                   = smart_obj__copy,
              instantiate_handler            = smart_obj__instantiate,
              compare_handler                = smart_obj__compare,
              serialize_handler              = smart_obj__save,
              load_derive_handler            = smart_obj__load,
              to_string_handler              = smart_obj__to_string);
class smart_object
// clang-format on
{
    KRG_gen_meta__smart_object();

public:
    struct construct_params : public base_construct_params
    {
    };

    KRG_gen_class_meta_super(smart_object);

    KRG_gen_meta_api;

    friend class core::object_constructor;

    template <typename T>
    bool
    castable_to()
    {
        static_assert(std::is_base_of<smart_object, T>::value, "Non a smart object!");

        return dynamic_cast<T*>(this);
    }

    template <typename T>
    T*
    as()
    {
        KRG_check(this, "Doesn't make sense to call as on dead object right?");

        if (!castable_to<T>())
        {
            return nullptr;
        }

        return (T*)this;
    }

    template <typename T>
    T&
    asr()
    {
        KRG_check(this, "Doesn't make sense to call as on dead object right?");
        return *((T*)this);
    }

    uint8_t*
    as_blob() const
    {
        KRG_check(this, "Doesn't make sense to call as on dead object right?");
        return (uint8_t*)this;
    }

    bool
    construct(const this_class::construct_params&)
    {
        return true;
    }

    // CDO construction — override in derived types for custom CDO setup.
    // Default delegates to construct().
    bool
    construct_default(const this_class::construct_params& p)
    {
        return construct(p);
    }

    virtual bool
    post_construct();

    virtual bool
    post_load();

    const smart_object*
    get_class_obj() const
    {
        return m_proto_obj;
    }

    const core::package*
    get_package() const
    {
        return m_package;
    }

    const core::level*
    get_level() const
    {
        return m_level;
    }

    smart_object_state
    get_state()
    {
        return m_obj_state;
    }

    void
    set_state(smart_object_state v);

    smart_object_flags&
    get_flags()
    {
        return m_flags;
    }

    const smart_object_flags&
    get_flags() const
    {
        return m_flags;
    }

    void
    set_package(core::package* p)
    {
        m_package = p;
    }

    void
    set_level(core::level* p)
    {
        m_level = p;
    }

    core::architype
    get_architype_id() const;

    KRG_ar_function("category=reflection");
    const reflection::reflection_type*
    get_reflection() const
    {
        return m_rt;
    }

    const utils::id&
    get_type_id() const;

protected:
    void
    META_set_id(const utils::id& id)
    {
        m_id = id;
    }

    void
    META_set_class_obj(smart_object* obj)
    {
        m_proto_obj = obj;
    }

    void
    META_set_reflection_type(const reflection::reflection_type* rt)
    {
        m_rt = rt;
    }

    const reflection::reflection_type* m_rt = nullptr;

    KRG_ar_property("category=Meta", "access=read_only", "copyable=no");
    utils::id m_id;

    const smart_object* m_proto_obj = nullptr;
    core::package* m_package = nullptr;
    core::level* m_level = nullptr;

    smart_object_state m_obj_state = smart_object_state::empty;
    smart_object_flags m_flags;

    // True after render commands have been built for this object.
    // Used by builders to decide create vs update. Not serialized.
    bool m_render_built = false;

public:
    bool
    get_render_built() const
    {
        return m_render_built;
    }

    void
    set_render_built(bool v)
    {
        m_render_built = v;
    }
};

template <typename To, typename From>
std::shared_ptr<To>
cast_ref(const std::shared_ptr<From>& ref)
{
    static_assert(std::is_base_of<smart_object, From>::value, "Non a smart object!");
    static_assert(std::is_base_of<smart_object, To>::value, "Non a smart object!");

    if (!ref || !ref->castable_to<To>())
    {
        return nullptr;
    }

    return std::static_pointer_cast<To>(ref);
}

template <typename To, typename From>
To*
cast_ref(From* ref)
{
    static_assert(std::is_base_of<smart_object, From>::value, "Non a smart object!");
    static_assert(std::is_base_of<smart_object, To>::value, "Non a smart object!");

    if (!ref || !ref->castable_to<To>())
    {
        return nullptr;
    }

    return (To*)ref;
}

}  // namespace root
}  // namespace kryga
