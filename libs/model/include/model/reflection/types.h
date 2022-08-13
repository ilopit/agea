#pragma once

#include <type_traits>

#include "model/model_minimal.h"

#include "model/model_fwds.h"

#define AGEA_create_resolver(in_type, out_type)            \
    template <>                                            \
    static property_type_description resolve<in_type>()    \
    {                                                      \
        return {out_type, false};                          \
    }                                                      \
                                                           \
    template <>                                            \
    static property_type_description resolve<in_type##*>() \
    {                                                      \
        return {out_type, true};                           \
    }

#define AGEA_create_resolver_simple(in_type, out_type)  \
    template <>                                         \
    static property_type_description resolve<in_type>() \
    {                                                   \
        return {out_type, false};                       \
    }

namespace agea
{

namespace reflection
{

enum class property_type
{
    t_nan = 0,

    t_str,
    t_id,

    t_bool,
    t_float,

    t_i8,
    t_i16,
    t_i32,
    t_i64,

    t_u8,
    t_u16,
    t_u32,
    t_u64,

    t_f,
    t_d,

    t_vec3,

    t_txt,
    t_mat,
    t_msh,
    t_obj,
    t_com,

    t_last = t_com + 1
};

struct property_type_description
{
    property_type_description() = default;

    property_type_description(property_type t, bool ptr)
        : type(t)
        , is_ptr(ptr)
    {
    }

    property_type_description(property_type t, bool ptr, bool collection)
        : type(t)
        , is_ptr(ptr)
        , is_collection(collection)
    {
    }

    property_type type = property_type::t_nan;
    bool is_ptr = false;
    bool is_collection = false;
};

template <typename T>
struct is_std_vector : std::false_type
{
    static property_type_description
    type()
    {
        return type_resolver::resolve<T>();
    }
};

template <typename T, typename A>
struct is_std_vector<std::vector<T, A>> : std::true_type
{
    static property_type_description
    type()
    {
        auto ptd = type_resolver::resolve<T>();
        ptd.is_collection = true;
        return ptd;
    }
};

template <typename T, typename A>
property_type_description
resolve2(std::vector<T, A>&)
{
    return resolve<T>()
}

template <typename T>
property_type_description
resolve2(T&)
{
    return {};
}

struct type_resolver
{
    template <typename T>
    static property_type_description
    resolve()
    {
        if (is_std_vector<T>::value)
        {
            return is_std_vector<T>::type();
        }
        else if (std::is_base_of<::agea::model::material, std::remove_pointer<T>::type>::value)
        {
            return property_type_description(property_type::t_mat, false);
        }
        else if (std::is_base_of<::agea::model::texture, std::remove_pointer<T>::type>::value)
        {
            return property_type_description(property_type::t_txt, false);
        }
        else if (std::is_base_of<::agea::model::mesh, std::remove_pointer<T>::type>::value)
        {
            return property_type_description(property_type::t_msh, false);
        }
        else if (std::is_base_of<::agea::model::component, std::remove_pointer<T>::type>::value)
        {
            return property_type_description(property_type::t_com, false);
        }
        else if (std::is_base_of<::agea::model::smart_object, std::remove_pointer<T>::type>::value)
        {
            return property_type_description(property_type::t_obj, false);
        }

        return property_type_description(property_type::t_nan, false);
    }

    AGEA_create_resolver(std::string, property_type::t_str);
    AGEA_create_resolver(utils::id, property_type::t_id);

    AGEA_create_resolver(bool, property_type::t_bool);

    AGEA_create_resolver(std::int8_t, property_type::t_i8);
    AGEA_create_resolver(std::int16_t, property_type::t_i16);
    AGEA_create_resolver(std::int32_t, property_type::t_i32);
    AGEA_create_resolver(std::int64_t, property_type::t_i64);

    AGEA_create_resolver(std::uint8_t, property_type::t_u8);
    AGEA_create_resolver(std::uint16_t, property_type::t_u16);
    AGEA_create_resolver(std::uint32_t, property_type::t_u32);
    AGEA_create_resolver(std::uint64_t, property_type::t_u64);

    AGEA_create_resolver(float, property_type::t_f);
    AGEA_create_resolver(double, property_type::t_d);

    AGEA_create_resolver(model::vec3, property_type::t_vec3);
};

}  // namespace reflection
}  // namespace agea
