#pragma once

#include "model/object_constructor.h"

#include "reflection/types.h"
#include "reflection/property.h"
#include "reflection/object_reflection.h"

#include "core/agea_minimal.h"

#include <string>
#include <memory>

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

#define AGEA_gen_meta_api                                           \
    friend class ::agea::reflection::entry;                         \
                                                                    \
    void META_class_set_type_id();                                  \
                                                                    \
    static const char* META_type_id();                              \
                                                                    \
    static reflection::object_reflection* META_object_reflection(); \
                                                                    \
    virtual reflection::object_reflection* reflection() const;      \
                                                                    \
    virtual bool META_construct(smart_object::construct_params& i); \
                                                                    \
    virtual bool META_post_construct();                             \
                                                                    \
    virtual std::shared_ptr<smart_object> META_create_empty_obj();  \
                                                                    \
    static std::shared_ptr<this_class> META_class_create_empty_obj();

namespace agea
{
namespace model
{

class objects_cache;

class smart_object
{
public:
    struct construct_params
    {
        std::string id;
        std::string name;
    };

    AGEA_gen_class_meta_super(smart_object);
    AGEA_gen_meta_api;

    const std::string&
    type_id() const
    {
        return m_type_id;
    }

    const std::string&
    id() const
    {
        return m_id;
    }

    void
    META_set_id(const std::string& id)
    {
        m_id = id;
    }

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

    bool
    construct(this_class::construct_params&)
    {
        return true;
    }

    bool
    post_construct();

    virtual void
    editor_update()
    {
    }

    void
    set_class_obj(smart_object* obj)
    {
        m_class_obj = obj;
    }

    AGEA_property("category=meta", "visible=true");
    std::string m_type_id;

    AGEA_property("category=meta", "serializable=true", "visible=true", "copyable=no");
    std::string m_id;

    smart_object* m_class_obj = nullptr;
    objects_cache* m_cache = nullptr;
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
