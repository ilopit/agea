#pragma once

#include <type_traits>

namespace agea
{
namespace utils
{

template <typename method_return_type, typename... method_input_args>
struct generic_event_handler
{
    using dummy_object_type = generic_event_handler<method_return_type, method_input_args...>;
    using dummy_method_type = method_return_type (dummy_object_type::*)(method_input_args...);

    dummy_object_type* obj = nullptr;
    dummy_method_type method = nullptr;

    template <typename subcribe_obj, typename subscribe_method>
    void
    assign(subcribe_obj* o, subscribe_method m)
    {
        using subscribe_method_return_type =
            std::invoke_result<decltype(m), subcribe_obj, method_input_args...>::type;

        static_assert(std::is_same<subscribe_method_return_type, method_return_type>::value,
                      "Return is different!");

        obj = (dummy_object_type*)o;
        method = (dummy_method_type)m;
    }

    template <typename... method_input_args>
    method_return_type
    fire(method_input_args&&... args)
    {
        if constexpr (std::is_same<void, method_return_type>::value)
        {
            (*obj.*method)(std::forward<method_input_args>(args)...);
        }
        else
        {
            return (*obj.*method)(std::forward<method_input_args>(args)...);
        }
    }
};

}  // namespace utils
}  // namespace agea