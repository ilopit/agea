#pragma once

#include <type_traits>

#include "type_description.h"

namespace agea::reflection
{
template <typename T>
struct type_resolver
{
    enum
    {
        value = -1
    };
};

template <typename T>
type_description
agea_type_resolve()
{
    if constexpr (std::is_pointer<T>::value)
    {
        using no_ptr_type = std::remove_pointer<T>::type;
        return type_description{type_resolver<no_ptr_type>::value, false};
    }

    return type_description{type_resolver<T>::value, false};
}

}  // namespace agea::reflection