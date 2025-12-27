#pragma once

#include "serialization/serialization.h"

namespace agea
{

namespace serialization
{

struct utils
{
    template <typename T>
    static void
    read_if_exists(const std::string& key, container& s, T& p)
    {
        if (s[key].IsDefined())
        {
            p = s[key].as<T>();
        }
    }

    static bool
    has_key(const std::string& key, container& s)
    {
        if (!s[key])
        {
            return false;
        }
        return true;
    }

    template <typename T>
    static bool
    read(const std::string& key, container& s, T& p)
    {
        if (!s[key])
        {
            return false;
        }

        p = s[key].as<T>();

        return true;
    }

    template <typename T>
    static bool
    read_3vec(const std::string& key,
              container& s,
              const std::string& akey,
              const std::string& bkey,
              const std::string& ckey,
              T& a,
              T& b,
              T& c)
    {
        if (!s[key].IsDefined())
        {
            return false;
        }

        auto& e = s[key];

        a = e[akey].as<T>();
        b = e[bkey].as<T>();
        c = e[ckey].as<T>();

        return true;
    }
};

}  // namespace serialization
}  // namespace agea
