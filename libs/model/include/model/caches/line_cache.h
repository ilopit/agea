#pragma once

#include "model/model_minimal.h"
#include "root/smart_object.h"

#include <utils/line_conteiner.h>

#include <type_traits>

template <typename T>
struct is_shared_ptr : std::false_type
{
    typedef void Type;
};

template <typename T>
struct is_shared_ptr<std::shared_ptr<T>> : std::true_type
{
    typedef T Type;
};

template <typename T>
struct is_unique_ptr : std::false_type
{
    typedef void Type;
};

template <typename T>
struct is_unique_ptr<std::unique_ptr<T>> : std::true_type
{
    typedef T Type;
};

namespace agea
{
namespace model
{

template <typename T>
class line_cache : public utils::line_conteiner<T>
{
public:
    line_cache()
    {
        static_assert(
            (is_shared_ptr<T>::value &&
             std::is_base_of<root::smart_object, typename is_shared_ptr<T>::Type>::value) ||
                (std::is_pointer<T>::value &&
                 std::is_base_of<root::smart_object, std::remove_pointer<T>::type>::value),
            "False");
    }

    T*
    get_item(const utils::id& id)
    {
        auto itr = find_if([&id](const T& o) { return o->get_id() == id; });

        if (itr == std::is_pointer<T>::value)
        {
            return &(*itr);
        }
        else
        {
            return itr->get();
        }
    }

    bool
    has_item(const utils::id& id)
    {
        return (bool)get_item(id);
    }
};
}  // namespace model
}  // namespace agea