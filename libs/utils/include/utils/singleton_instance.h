#pragma once

#include "utils/check.h"

#include "utils/base_singleton_instance.h"
#include "utils/singleton_registry.h"

#include <memory>

namespace agea
{
template <typename T, typename S>
class singleton_instance : public base_singleton_instance
{
public:
    using type = singleton_instance<T, S>;

    static void
    create(singleton_registry& r)
    {
        auto obj_ptr = new T();

        s_instance.m_obj = obj_ptr;
        s_instance.m_reg = &r;

        s_instance.add();
    }

    static void
    create_ref(T* obj_ptr)
    {
        s_instance.m_obj = obj_ptr;
        s_instance.m_reg = nullptr;
    }

    static void
    create(singleton_registry& r, T&& obj)
    {
        auto obj_ptr = new T(std::move(obj));

        s_instance.m_obj = obj_ptr;
        s_instance.m_reg = &r;

        s_instance.add();
    }

    template <typename... Args>
    static void
    create(singleton_registry& r, Args... args)
    {
        auto obj_ptr = new T(std::forward<Args>(args)...);

        s_instance.m_obj = obj_ptr;
        s_instance.m_reg = &r;

        s_instance.add();
    }

    static T*
    get()
    {
        AGEA_check(s_instance.m_obj, "Instance is empty");
        return s_instance.m_obj;
    }

    static T&
    getr()
    {
        AGEA_check(s_instance.m_obj, "Instance is empty");
        return *s_instance.m_obj;
    }

    void
    reset()
    {
        s_instance.m_obj = nullptr;
        s_instance.m_reg = nullptr;
    }

protected:
    T* m_obj = nullptr;
    static singleton_instance s_instance;
};
}  // namespace agea