#pragma once

#include "utils/check.h"
#include "utils/agea_log.h"

#include <memory>

namespace agea
{
struct base_closure
{
    virtual ~base_closure()
    {
    }
};

template <typename T>
struct closure : public base_closure
{
    closure(std::unique_ptr<T>&& obj, T*& parent)
        : m_obj(std::move(obj))
        , m_parent(parent)
    {
    }

    ~closure()
    {
        m_parent = nullptr;
    }

private:
    std::unique_ptr<T> m_obj;
    T*& m_parent;
};

#define AGEA_SINGLETONE_SEED __COUNTER__

template <typename T, int k = 1>
class weird_singleton
{
public:
    template <typename... Args>
    static std::unique_ptr<closure<T>>
    create(Args... args)
    {
        AGEA_check(!s_obj, "instance is not empty");

        auto obj = std::make_unique<T>(std::forward<Args>(args)...);
        s_obj = obj.get();

        return std::make_unique<closure<T>>(std::move(obj), s_obj);
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

private:
    inline static T* s_obj = nullptr;
};

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