#pragma once

#include <string>

#include <json/json.h>

namespace Json
{
class Value;
}

namespace agea
{

namespace serialization
{
using json_conteiner = Json::Value;

struct game_object_serialization_helper
{
    template <typename T>
    static void
    read_if_exists(const std::string& key, json_conteiner& s, T& p)
    {
        if (s.isMember(key))
        {
            p = s[key].as<T>();
        }
    }

    static bool
    has_key(const std::string& key, json_conteiner& s)
    {
        if (!s.isMember(key))
        {
            return false;
        }
    }

    template <typename T>
    static bool
    read(const std::string& key, json_conteiner& s, T& p)
    {
        if (!s.isMember(key))
        {
            return false;
        }

        p = s[key].as<T>();

        return true;
    }

    template <typename T>
    static bool
    read_3vec(const std::string& key,
              json_conteiner& s,
              const std::string& akey,
              const std::string& bkey,
              const std::string& ckey,
              T& a,
              T& b,
              T& c)
    {
        if (!s.isMember(key))
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
