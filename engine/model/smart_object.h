#pragma once

#include "model/object_prototypes_registry.h"
#include "model/object_constructor.h"

#include "reflection/types.h"
#include "reflection/property.h"

#include "agea_minimal.h"

#include <string>
#include <memory>

#define AGEA_gen_class_name(t)         \
    static const char* META_class_id() \
    {                                  \
        return AGEA_stringify(t);      \
    }

#define AGEA_gen_class_meta_super(t) \
    AGEA_gen_class_name(t);          \
    using this_class = t;            \
    using base_class = t;            \
    t() = default;                   \
    t(t&) = delete;                  \
    t& operator=(t&) = delete;

#define AGEA_gen_class_meta(t, b)     \
    AGEA_gen_class_name(t);           \
    using this_class = t;             \
    using base_class = b::this_class; \
    t() = default;                    \
    t(t&) = delete;                   \
    t& operator=(t&) = delete;

#define AGEA_gen_construct_params struct construct_params : base_class::construct_params

#define AGEA_gen_meta_api                                                     \
                                                                              \
    virtual const char* class_id() const                                      \
    {                                                                         \
        return this_class::META_class_id();                                   \
    }                                                                         \
                                                                              \
    void META_class_set_class_id()                                            \
    {                                                                         \
        m_class_id = META_class_id();                                         \
    }                                                                         \
                                                                              \
    static reflection::class_reflection_table* META_class_reflection_table()  \
    {                                                                         \
        static reflection::class_reflection_table rt{                         \
            std::is_same<this_class, base_class>::value                       \
                ? nullptr                                                     \
                : base_class::META_class_reflection_table()};                 \
        return &rt;                                                           \
    }                                                                         \
                                                                              \
    virtual reflection::class_reflection_table* reflection_table() const      \
    {                                                                         \
        return this_class::META_class_reflection_table();                     \
    }                                                                         \
                                                                              \
    friend smart_object_prototype_registrator;                                \
    inline static smart_object_prototype_registrator<this_class> s_register;  \
                                                                              \
    virtual bool META_construct(smart_object::construct_params& i)            \
    {                                                                         \
        /* Replace to dynamic cast */                                         \
        this_class::construct_params* cp = (this_class::construct_params*)&i; \
                                                                              \
        return construct(*cp);                                                \
    }                                                                         \
                                                                              \
    virtual bool META_serialize(json_conteiner& c)                            \
    {                                                                         \
        return serialize(c);                                                  \
    }                                                                         \
                                                                              \
    virtual bool META_deserialize(json_conteiner& c)                          \
    {                                                                         \
        return deserialize(c);                                                \
    }                                                                         \
                                                                              \
    virtual bool META_deserialize_finalize(json_conteiner& c)                 \
    {                                                                         \
        return deserialize_finalize(c);                                       \
    }                                                                         \
                                                                              \
    virtual bool META_post_construct()                                        \
    {                                                                         \
        return post_construct();                                              \
    }                                                                         \
                                                                              \
    virtual std::shared_ptr<smart_object> META_create_empty_obj()             \
    {                                                                         \
        return this_class::META_class_create_empty_obj();                     \
    }                                                                         \
                                                                              \
    virtual std::shared_ptr<smart_object> META_clone_obj()                    \
    {                                                                         \
        auto obj = META_class_create_empty_obj();                             \
        obj->clone(*this);                                                    \
        return obj;                                                           \
    }                                                                         \
                                                                              \
    static std::shared_ptr<this_class> META_class_create_empty_obj()          \
    {                                                                         \
        auto s = std::make_shared<this_class>();                              \
        s->META_class_set_class_id();                                         \
        return s;                                                             \
    }

namespace agea
{
namespace model
{
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

    bool clone(this_class& src);

    // protected:
    bool
    construct(this_class::construct_params& p)
    {
        return true;
    }

    bool serialize(json_conteiner& c);
    bool deserialize(json_conteiner& c);
    bool deserialize_finalize(json_conteiner& c);
    bool post_construct();

    std::string m_class_id;
    std::string m_id;
};

AGEA_make_property(smart_object, m_class_id, ::agea::reflection::access_mode::ro, "meta");
AGEA_make_property(smart_object, m_id, ::agea::reflection::access_mode::ro, "meta");

template <typename To, typename From>
std::shared_ptr<To>
cast_ref(std::shared_ptr<From> ref)
{
    static_assert(std::is_base_of<smart_object, From>::value, "Non a smart object!");
    static_assert(std::is_base_of<smart_object, To>::value, "Non a smart object!");

    if (!ref || !ref->castable_to<To>())
    {
        return nullptr;
    }

    return std::static_pointer_cast<To>(ref);
}

}  // namespace model
}  // namespace agea
