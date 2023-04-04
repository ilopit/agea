#pragma once

#include "model/model_fwds.h"
#include "model/model_minimal.h"
#include "model/model_module.h"

#include <utils/buffer.h>
#include <utils/id.h>

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

namespace agea
{
namespace reflection
{

struct type_resolver;

struct property_type_description
{
    property_type_description() = default;

    property_type_description(::agea::utils::id t, bool ptr)
        : type(t)
        , is_ptr(ptr)
    {
    }

    property_type_description(::agea::utils::id t, bool ptr, bool collection)
        : type(t)
        , is_ptr(ptr)
        , is_collection(collection)
    {
    }

    ::agea::utils::id type;
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
        if (std::is_base_of<::agea::model::material, typename std::remove_pointer<T>::type>::value)
        {
            return property_type_description{model::types::tid_mat, false};
        }
        else if (std::is_base_of<::agea::model::texture,
                                 typename std::remove_pointer<T>::type>::value)
        {
            return property_type_description{model::types::tid_txt, false};
        }
        else if (std::is_base_of<::agea::model::mesh, typename std::remove_pointer<T>::type>::value)
        {
            return property_type_description{model::types::tid_msh, false};
        }
        else if (std::is_base_of<::agea::model::component,
                                 typename std::remove_pointer<T>::type>::value)
        {
            return property_type_description{model::types::tid_com, false};
        }
        else if (std::is_base_of<::agea::model::shader_effect,
                                 typename std::remove_pointer<T>::type>::value)
        {
            return property_type_description{model::types::tid_se, false};
        }
        else if (std::is_base_of<::agea::model::smart_object,
                                 typename std::remove_pointer<T>::type>::value)
        {
            return property_type_description{model::types::tid_obj, false};
        }

        return property_type_description{};
    }

    AGEA_create_resolver(std::string, model::types::tid_string);
    AGEA_create_resolver(utils::id, model::types::tid_id);
    AGEA_create_resolver(utils::buffer, model::types::tid_buffer);
    AGEA_create_resolver(model::color, model::types::tid_color);

    AGEA_create_resolver(bool, model::types::tid_bool);

    AGEA_create_resolver(std::int8_t, model::types::tid_i8);
    AGEA_create_resolver(std::int16_t, model::types::tid_i16);
    AGEA_create_resolver(std::int32_t, model::types::tid_i32);
    AGEA_create_resolver(std::int64_t, model::types::tid_i64);

    AGEA_create_resolver(std::uint8_t, model::types::tid_u8);
    AGEA_create_resolver(std::uint16_t, model::types::tid_u16);
    AGEA_create_resolver(std::uint32_t, model::types::tid_u32);
    AGEA_create_resolver(std::uint64_t, model::types::tid_u64);

    AGEA_create_resolver(float, model::types::tid_float);
    AGEA_create_resolver(double, model::types::tid_double);

    AGEA_create_resolver(model::vec2, model::types::tid_vec2);
    AGEA_create_resolver(model::vec3, model::types::tid_vec3);
    AGEA_create_resolver(model::vec4, model::types::tid_vec4);
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
