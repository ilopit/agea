#pragma once

#include "root/smart_object.generated.h"

#include "core/reflection/reflection_type.h"
#include "core/model_minimal.h"
#include "core/architype.h"

#include <ar/ar_defines.h>

#include <string>
#include <memory>
#include <bitset>

#define AGEA_gen_class_meta_super(t) \
    using this_class = t;            \
    using base_class = t;            \
    t();                             \
    virtual ~t();                    \
    t(t&) = delete;                  \
    t& operator=(t&) = delete;

#define AGEA_gen_class_meta(t, b)     \
    using this_class = t;             \
    using base_class = b::this_class; \
    t();                              \
    ~t();                             \
    t(t&) = delete;                   \
    t& operator=(t&) = delete;

#define AGEA_gen_construct_params struct construct_params : base_class::construct_params

#define AGEA_gen_meta_api                                                                     \
                                                                                              \
    static ::agea::utils::id AR_TYPE_id();                                                    \
                                                                                              \
    static const ::agea::reflection::reflection_type& AR_TYPE_reflection();                   \
                                                                                              \
    virtual bool META_construct(const ::agea::root::smart_object::construct_params& i);       \
                                                                                              \
    static std::shared_ptr<this_class> AR_TYPE_create_empty_obj(const ::agea::utils::id& id); \
                                                                                              \
    static std::shared_ptr<::agea::root::smart_object> AR_TYPE_create_empty_gen_obj(          \
        const ::agea::utils::id& id);

namespace agea
{
namespace core
{
class object_constructor;
class package;

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

enum class smart_object_state
{
    empty = 0,
    loaded,
    constructed,
    render_preparing,
    render_ready
};

enum smart_object_state_flag : uint32_t
{
    empty = 0,
    proto_obj = 1,
    instance_obj = 2,
    standalone = 4,
    inhereted = 8,
    mirror = 16,
    empty_obj = 32
};

using smart_object_ptr = std::shared_ptr<smart_object>;

AGEA_ar_class("architype=smart_object");
class smart_object
{
    AGEA_gen_meta__smart_object();

public:
    struct construct_params
    {
    };

    AGEA_gen_class_meta_super(smart_object);

    AGEA_gen_meta_api;

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
        return *((T*)this);
    }

    blob_ptr
    as_blob() const
    {
        return (blob_ptr)this;
    }

    bool
    construct(const this_class::construct_params&)
    {
        return true;
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

    void
    set_flag(uint32_t f)
    {
        m_obj_internal_state |= f;
    }

    bool
    has_flag(uint32_t f) const
    {
        return m_obj_internal_state & f;
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

    AGEA_ar_function("category=reflection");
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


    AGEA_ar_property("category=meta", "access=read_only", "copyable=no");
    utils::id m_id;

    const smart_object* m_proto_obj = nullptr;
    core::package* m_package = nullptr;
    core::level* m_level = nullptr;

    smart_object_state m_obj_state = smart_object_state::empty;
    uint32_t m_obj_internal_state = smart_object_state_flag::empty;
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
}  // namespace agea
