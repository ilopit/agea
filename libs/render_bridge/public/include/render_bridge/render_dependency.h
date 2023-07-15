#pragma once

#include <unordered_map>
#include <unordered_set>

#include <core/model_minimal.h>

namespace agea
{

namespace root
{
class smart_object;
}

class node
{
public:
    void
    add(root::smart_object* obj)
    {
        if (obj)
        {
            m_children.insert(obj);
        }
    }

    std::unordered_set<root::smart_object*>&
    get_children()
    {
        return m_children;
    }

    root::smart_object*
    get_obj()
    {
        return m_obj;
    }

    void
    reset(root::smart_object* v)
    {
        m_prev_children = m_children;
        m_children.clear();
        m_obj = v;
    }

    root::smart_object* m_obj = nullptr;
    std::unordered_set<root::smart_object*> m_children;
    std::unordered_set<root::smart_object*> m_prev_children;
};

class render_object_dependency_graph
{
public:
    node&
    get_node(root::smart_object* obj);

    void
    build_node(root::smart_object* obj);

    void
    print(bool top_down);

private:
    std::unordered_map<root::smart_object*, node> m_top_down;
    std::unordered_map<root::smart_object*, node> m_down_top;
};

}  // namespace agea