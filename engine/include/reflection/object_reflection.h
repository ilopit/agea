#pragma once

#include "reflection/property.h"

#include <vector>

namespace agea
{
namespace reflection
{
class object_reflection
{
public:
    using property_list = std::vector<std::shared_ptr<property>>;

    object_reflection(object_reflection* p, const char* id)
        : parent(p)
        , class_id(id)
    {
        types().push_back(this);
    }

    static std::vector<object_reflection*>&
    types()
    {
        static std::vector<object_reflection*> registered_classes;
        return registered_classes;
    }

    static void
    fill_properties();

    object_reflection* parent = nullptr;

    std::unordered_map<std::string, property_list> m_editor_properties;
    property_list m_properties;
    property_list m_serilalization_properties;
    bool initialized = false;
    const char* class_id = nullptr;
};
}  // namespace reflection
}  // namespace agea