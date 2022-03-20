#pragma once

#include "reflection/types.h"

#include <string>
#include <vector>
#include <unordered_map>

namespace agea
{
namespace reflection
{
class property
{
public:
    property(size_t o, access_mode m, supported_type t, std::string n, std::string c)
        : ofsset(o)
        , mode(m)
        , type(t)
        , name(std::move(n))
        , categoty(std::move(c))
    {
    }

    size_t ofsset;
    access_mode mode;
    supported_type type;
    std::string name;
    std::string categoty;
};

template <typename T>
struct property_registrator
{
    property_registrator(size_t type_offset,
                         std::string field_name,
                         supported_type st,
                         access_mode am,
                         std::string category)
    {
        auto table = T::META_class_reflection_table();

        // TODO, Meh :(
        if ((field_name.size() > 2) && field_name[0] == 'm' && field_name[1] == '_')
        {
            field_name.erase(field_name.begin(), field_name.begin() + 2);
        }

        table->properties.emplace_back(type_offset, am, st, std::move(field_name),
                                       std::move(category));
    }
};

struct class_reflection_table
{
    class_reflection_table()
        : parent(nullptr)
    {
    }

    class_reflection_table(class_reflection_table* p)
        : parent(p)
    {
    }
    class_reflection_table* parent;
    std::vector<property> properties;
};

}  // namespace reflection
}  // namespace agea

#define AGEA_make_property(class_type, field, readonly, category)                                \
                                                                                                 \
    inline ::agea::reflection::property_registrator<class_type> AGEA_concat3(META_reflection_,   \
                                                                             class_type, field)  \
    {                                                                                            \
        offsetof(class_type, field), AGEA_stringify(field),                                      \
            ::agea::reflection::type_resolver::resolve<decltype(class_type::field)>(), readonly, \
            category                                                                             \
    }