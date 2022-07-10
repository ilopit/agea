#include "model/reflection/object_reflection.h"

#include <stack>

namespace agea::reflection
{

void
object_reflection::fill_properties()
{
    auto& types_list = types();

    for (auto itr = types_list.rbegin(); itr != types_list.rend(); ++itr)
    {
        object_reflection* cr = *itr;
        std::stack<object_reflection*> to_handle;

        while (cr)
        {
            to_handle.push(cr);

            if (cr->initialized)
            {
                break;
            }
            cr = cr->parent;
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

    for (auto& c : types_list)
    {
        for (auto& p : c->m_properties)
        {
            if (p->visible)
            {
                c->m_editor_properties[p->category].push_back(p);
            }
            if (p->types_serialization_handler)
            {
                c->m_serilalization_properties.push_back(p);
            }
        }
    }
}

}  // namespace agea::reflection