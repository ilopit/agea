#include "core/reflection/reflection_type.h"

#include <stack>

namespace agea
{
glob::reflection_type_registry::type glob::reflection_type_registry::s_instance;

void
reflection::reflection_type_registry::finilaze()
{
    for (auto& t : m_types)
    {
        auto* rt = &t.second;
        std::stack<reflection_type*> to_handle;

        while (rt)
        {
            to_handle.push(rt);

            if (rt->initialized)
            {
                break;
            }
            rt = rt->parent;
        }

        property_list to_insert;

        while (!to_handle.empty())
        {
            auto& top = to_handle.top();

            top->m_properties.insert(top->m_properties.end(), to_insert.begin(), to_insert.end());

            to_insert = top->m_properties;
            top->initialized = true;

            top->finalize_handlers();

            to_handle.pop();
        }
    }

    for (auto& c : m_types)
    {
        for (auto& p : c.second.m_properties)
        {
            if (p->serializable)
            {
                c.second.m_serilalization_properties.push_back(p);
            }
        }
    }
}

void
reflection::reflection_type::finalize_handlers()
{
    if (parent)
    {
        serialization = serialization ? serialization : parent->serialization;
        deserialization = deserialization ? deserialization : parent->deserialization;
        deserialization_with_proto = deserialization_with_proto
                                         ? deserialization_with_proto
                                         : parent->deserialization_with_proto;

        copy = copy ? copy : parent->copy;
        compare = compare ? compare : parent->compare;
        ui = ui ? ui : parent->ui;
        render = render ? render : parent->render;
    }
}

}  // namespace agea