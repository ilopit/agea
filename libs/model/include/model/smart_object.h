#pragma once

#include "smart_object.generated.h"

#include "model/reflection/object_reflection.h"
#include "model/model_minimal.h"

#include <arl/arl_defines.h>

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

#define AGEA_gen_meta_api                                                 \
    friend class ::agea::reflection::entry;                               \
                                                                          \
    void META_class_set_type_id();                                        \
                                                                          \
    void META_class_set_architype_id();                                   \
                                                                          \
    static utils::id META_type_id();                                      \
                                                                          \
    static reflection::object_reflection* META_object_reflection();       \
                                                                          \
    virtual const reflection::object_reflection* reflection() const;      \
                                                                          \
    virtual bool META_construct(const smart_object::construct_params& i); \
                                                                          \
    virtual bool META_post_construct();                                   \
                                                                          \
    virtual std::shared_ptr<smart_object> META_create_empty_obj();        \
                                                                          \
    static std::shared_ptr<this_class> META_class_create_empty_obj();

#define AGEA_gen_meta_architype_api(a)                     \
    AGEA_gen_meta_api static architype META_architype_id() \
    {                                                      \
        return architype::a;                               \
    }

namespace agea
{
namespace model
{

class object_constructor;
class package;
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

AGEA_class();
class smart_object
{
    AGEA_gen_meta__smart_object();

public:
    struct construct_params
    {
    };

    AGEA_gen_class_meta_super(smart_object);
    AGEA_gen_meta_architype_api(smart_object);

    friend class object_constructor;

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

    bool
    post_construct();

    const smart_object*
    get_class_obj() const
    {
        return m_proto_obj;
    }

    const package*
    get_package() const
    {
        return m_package;
    }

    smart_object_state
    get_state()
    {
        return m_obj_state;
    }

    void
    set_state(smart_object_state v)
    {
        m_obj_state = v;
    }

    void
    set_state_flag(uint32_t f)
    {
        m_obj_internal_state |= f;
    }

    bool
    has_state_flag(uint32_t f) const
    {
        return m_obj_internal_state & f;
    }

    void
    set_package(package* p)
    {
        m_package = p;
    }

    void
    set_level(level* p)
    {
        m_level = p;
    }

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

    AGEA_property("category=meta", "access=read_only");
    architype m_architype_id = architype::unknown;

    AGEA_property("category=meta", "access=read_only", "copyable=no");
    utils::id m_type_id;

    AGEA_property("category=meta", "access=read_only", "copyable=no");
    utils::id m_id;

    const smart_object* m_proto_obj = nullptr;
    package* m_package = nullptr;
    level* m_level = nullptr;

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

}  // namespace model
}  // namespace agea
