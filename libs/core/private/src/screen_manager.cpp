#include "core/screen_manager.h"

#include <utils/check.h>

namespace kryga::core
{

screen*
screen_manager::push(const utils::id& id)
{
    m_stack.push_back(std::make_unique<screen>(id));
    return m_stack.back().get();
}

void
screen_manager::pop()
{
    KRG_check(!m_stack.empty(), "pop() on empty screen stack");
    m_stack.back()->unload();
    m_stack.pop_back();
}

screen*
screen_manager::active()
{
    return m_stack.empty() ? nullptr : m_stack.back().get();
}

screen*
screen_manager::find(const utils::id& id)
{
    for (auto& s : m_stack)
    {
        if (s->get_id() == id)
        {
            return s.get();
        }
    }
    return nullptr;
}

void
screen_manager::tick(float dt)
{
    for (auto& s : m_stack)
    {
        s->tick(dt);
    }
}

void
screen_manager::clear()
{
    while (!m_stack.empty())
    {
        pop();
    }
}

}  // namespace kryga::core
