#pragma once

#include "utils/check.h"
#include "utils/agea_log.h"

#include <memory>

namespace agea
{
struct base_deleter
{
    virtual ~base_deleter()
    {
    }
};

template <typename T>
struct type_deleter : public base_deleter
{
    type_deleter(T** parent)
        : m_parent(parent)
    {
    }

    ~type_deleter()
    {
        if (*m_parent)
        {
            delete *m_parent;
            *m_parent = nullptr;
        }
    }

private:
    T** m_parent;
};

template <typename T, int k = 1>
class selfcleanable_singleton
{
public:
    template <typename... Args>
    static std::unique_ptr<base_deleter>
    create(Args... args)
    {
        AGEA_check(!s_obj, "instance is not empty");

        s_obj = new T(std::forward<Args>(args)...);

        return std::make_unique<type_deleter<T>>(&s_obj);
    }

    template <typename K>
    static void
    assign()
    {
        AGEA_check(!s_obj, "instance is not empty");
        s_obj = K::get();
    }

    static T*
    get()
    {
        AGEA_check(s_obj, "Instance is empty");
        return s_obj;
    }

    static T&
    getr()
    {
        AGEA_check(s_obj, "Instance is empty");
        return *s_obj;
    }

protected:
    inline static T* s_obj = nullptr;
};

using singletone_autodeleter = std::unique_ptr<agea::base_deleter>;

template <typename T, int seed = 1>
class simple_singleton
{
public:
    static void
    set(T obj)
    {
        m_obj = std::move(obj);
    }

    static T&
    get()
    {
        return m_obj;
    }

private:
    inline static T m_obj;
};

}  // namespace agea
