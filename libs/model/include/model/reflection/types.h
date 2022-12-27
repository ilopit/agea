#pragma once

#include "model/model_fwds.h"
#include "model/model_minimal.h"

#include <utils/buffer.h>

#include <type_traits>

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

struct type_resolver;

struct property_type_description
{
    property_type_description() = default;

    property_type_description(utils::agea_type t, bool ptr)
        : type(t)
        , is_ptr(ptr)
    {
    }

    property_type_description(utils::agea_type t, bool ptr, bool collection)
        : type(t)
        , is_ptr(ptr)
        , is_collection(collection)
    {
    }

    utils::agea_type type = utils::agea_type::t_nan;
    bool is_ptr = false;
    bool is_collection = false;
};

template <typename T>
struct is_std_vector;

template <typename T, typename A>
struct is_std_vector<std::vector<T, A>>;

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
        else if (std::is_base_of<::agea::model::material,
                                 typename std::remove_pointer<T>::type>::value)
        {
            return property_type_description(utils::agea_type::t_mat, false);
        }
        else if (std::is_base_of<::agea::model::texture,
                                 typename std::remove_pointer<T>::type>::value)
        {
            return property_type_description(utils::agea_type::t_txt, false);
        }
        else if (std::is_base_of<::agea::model::mesh, typename std::remove_pointer<T>::type>::value)
        {
            return property_type_description(utils::agea_type::t_msh, false);
        }
        else if (std::is_base_of<::agea::model::component,
                                 typename std::remove_pointer<T>::type>::value)
        {
            return property_type_description(utils::agea_type::t_com, false);
        }
        else if (std::is_base_of<::agea::model::shader_effect,
                                 typename std::remove_pointer<T>::type>::value)
        {
            return property_type_description(utils::agea_type::t_se, false);
        }
        else if (std::is_base_of<::agea::model::smart_object,
                                 typename std::remove_pointer<T>::type>::value)
        {
            return property_type_description(utils::agea_type::t_obj, false);
        }

        return property_type_description(utils::agea_type::t_nan, false);
    }

    AGEA_create_resolver(std::string, utils::agea_type::t_str);
    AGEA_create_resolver(utils::id, utils::agea_type::t_id);
    AGEA_create_resolver(utils::buffer, utils::agea_type::t_buf);
    AGEA_create_resolver(model::color, utils::agea_type::t_color);

    AGEA_create_resolver(bool, utils::agea_type::t_bool);

    AGEA_create_resolver(std::int8_t, utils::agea_type::t_i8);
    AGEA_create_resolver(std::int16_t, utils::agea_type::t_i16);
    AGEA_create_resolver(std::int32_t, utils::agea_type::t_i32);
    AGEA_create_resolver(std::int64_t, utils::agea_type::t_i64);

    AGEA_create_resolver(std::uint8_t, utils::agea_type::t_u8);
    AGEA_create_resolver(std::uint16_t, utils::agea_type::t_u16);
    AGEA_create_resolver(std::uint32_t, utils::agea_type::t_u32);
    AGEA_create_resolver(std::uint64_t, utils::agea_type::t_u64);

    AGEA_create_resolver(float, utils::agea_type::t_f);
    AGEA_create_resolver(double, utils::agea_type::t_d);

    AGEA_create_resolver(model::vec2, utils::agea_type::t_vec2);
    AGEA_create_resolver(model::vec3, utils::agea_type::t_vec3);
    AGEA_create_resolver(model::vec4, utils::agea_type::t_vec4);
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
        auto pdt = type_resolver::resolve<T>();
        pdt.is_collection = true;
        return pdt;
    }
};

}  // namespace reflection
}  // namespace agea
