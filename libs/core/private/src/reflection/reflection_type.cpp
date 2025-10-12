#include "core/reflection/reflection_type.h"

#include "core/reflection/reflection_type_utils.h"

#include <stack>

namespace agea
{

void
reflection::reflection_type_registry::add_type(reflection_type* t)
{
    m_types[t->type_id] = t;
    m_types_by_name[t->type_name] = t;
}

reflection::reflection_type*
reflection::reflection_type_registry::get_type(const agea::utils::id& id)
{
    auto itr = m_types_by_name.find(id);

    return itr != m_types_by_name.end() ? itr->second : nullptr;
}

reflection::reflection_type*
reflection::reflection_type_registry::get_type(const int id)
{
    auto itr = m_types.find(id);

    return itr != m_types.end() ? itr->second : nullptr;
}

void
reflection::reflection_type_registry::finilaze()
{
    for (auto& t : m_types)
    {
        auto* rt = t.second;
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
            top->override();

            to_handle.pop();
        }
    }

    for (auto& c : m_types)
    {
        for (auto& p : c.second->m_properties)
        {
            if (p->serializable)
            {
                c.second->m_serilalization_properties.push_back(p);
            }
            c.second->m_editor_properties[p->category].push_back(p);
        }
    }
}

#define AGEA_override_if_null(prop) prop = prop ? prop : parent->prop

void
reflection::reflection_type::override()
{
    if (parent)
    {
        if (arch == core::architype::unknown)
        {
            arch = parent->arch;
        }

        AGEA_override_if_null(serialize);
        AGEA_override_if_null(deserialize);
        AGEA_override_if_null(deserialize_with_proto);
        AGEA_override_if_null(copy);
        AGEA_override_if_null(compare);
        AGEA_override_if_null(to_string);
        AGEA_override_if_null(render_constructor);
        AGEA_override_if_null(render_destructor);
    }
}

std::string
reflection::reflection_type::as_string() const
{
    std::string result;

    result += std::format("{0}:{1}:{2}\n", module_id.cstr(), type_name.cstr(), type_id);
    result += "parent:" + (parent ? parent->type_name.str() : "no") + "\n";

    result += "properties:\n";

    for (auto p : m_properties)
    {
        result += std::format("  {0}:{1}:{2}\n",
                              p->rtype ? p->rtype->type_name.str() : std::string("custom"), p->name,
                              p->rtype ? p->rtype->type_id : -1);
    }
    result += "functions:\n";

    for (auto p : m_functions)
    {
        result += std::format("  {0}\n", p->name);
    }

    return result;
}

}  // namespace agea