#include "global_state/global_state.h"

#include <iostream>

namespace agea::glob
{

::agea::gs::state&
glob_state()
{
    static ::agea::gs::state g_glob_state;
    return g_glob_state;
}
void
glob_state_reset()
{
    glob_state() = {};
}
}  // namespace agea::glob

namespace agea::gs
{

state::state()
{
}

int
state::schedule_action(state_stage stage, scheduled_action action)
{
    auto& node = m_scheduled_actions[(size_t)stage];

    node.push_back(action);
    std::cerr << std::format("Scheduling action state {:x}  at {}, total {} \n", (size_t)this,
                             (size_t)stage, node.size());
    return (int)node.size();
}

void
state::run_create()
{
    AGEA_check(m_stage == state_stage::create, "Expected proper state!");
    run_items(m_stage);
    m_stage = state_stage::connect;
}

void
state::run_connect()
{
    AGEA_check(m_stage == state_stage::connect, "Expected proper state!");
    run_items(m_stage);
    m_stage = state_stage::init;
}

void
state::run_init()
{
    AGEA_check(m_stage == state_stage::init, "Expected proper state!");
    run_items(m_stage);
    m_stage = state_stage::ready;
}

void
state::run_items(state_stage stage)
{
    auto& node = m_scheduled_actions[(size_t)stage];
    for (auto& a : node)
    {
        a(*this);
    }

    node.clear();
}

}  // namespace agea::gs