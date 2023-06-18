#include <render_bridge/render_dependency.h>

#include <root/smart_object.h>
#include <core/reflection/reflection_type_utils.h>

#include <utils/agea_log.h>

namespace agea
{

agea::node&
render_object_dependency_graph::get_node(root::smart_object* obj)
{
    auto& n = m_top_down[obj];

    return n;
}

void
render_object_dependency_graph::build_node(root::smart_object* obj)
{
    auto& n = m_top_down[obj];

    n.reset(obj);

    for (auto& p : obj->get_reflection()->m_properties)
    {
        if (p->render_subobject)
        {
            auto dep = reflection::utils::as_type<root::smart_object*>(p->get_blob(*obj));
            if (dep)
            {
                n.add(dep);
            }
        }
    }

    for (auto a : n.m_children)
    {
        auto itr = n.m_prev_children.find(a);

        if (itr == n.m_prev_children.end())
        {
            m_down_top[a].add(obj);
            m_down_top[a].m_obj = a;
        }
    }

    for (auto r : n.m_prev_children)
    {
        auto itr = n.m_children.find(r);

        if (itr == n.m_children.end())
        {
            m_down_top[r].m_children.erase(obj);
            if (m_down_top[r].m_children.empty())
            {
                m_down_top.erase(r);
            }
        }
    }
}

void
render_object_dependency_graph::print(bool top_down)
{
    for (auto& d : (top_down ? m_top_down : m_down_top))
    {
        auto& node = d.second;

        if (!node.get_children().empty())
        {
            ALOG_INFO("{0}  =>", node.get_obj()->get_id().cstr());

            for (auto c : node.get_children())
            {
                ALOG_INFO("    {0}", c->get_id().cstr());
            }

            ALOG_INFO("    =====================");
        }
    }
}

}  // namespace agea