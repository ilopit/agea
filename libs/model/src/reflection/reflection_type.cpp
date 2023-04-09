#include "model/reflection/reflection_type.h"

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

}  // namespace agea