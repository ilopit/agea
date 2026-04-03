#include "global_state/global_state.h"

#include <utils/kryga_log.h>

#include <iostream>

namespace kryga::glob
{

::kryga::gs::state&
glob_state()
{
    static ::kryga::gs::state g_glob_state;
    return g_glob_state;
}
void
glob_state_reset()
{
    glob_state() = {};
}
}  // namespace kryga::glob

namespace kryga::gs
{

state_base_box::state_base_box(std::string name)
    : box_name(std::move(name))
{
    ALOG_TRACE("Box [{}] created ", box_name);
}

state_base_box::~state_base_box()
{
    ALOG_TRACE("Box [{}] destroyed ", box_name);
}

state::state()
{
}

state::~state()
{
    cleanup();
}

state&
state::operator=(state&& other)
{
    if (this != &other)
    {
        cleanup();
        m_stage = state_stage::create;
    }

    return *this;
}

void
state::cleanup()
{
    if (m_boxes.empty())
    {
        return;
    }

    ALOG_INFO("GS cleanup");
    while (!m_boxes.empty())
    {
        m_boxes.pop_back();
    }
}

int
state::schedule_action(state_stage stage, scheduled_action action)
{
    auto& node = m_scheduled_actions[(size_t)stage];

    node.push_back(action);
    std::cerr << std::format("Scheduling action state {:x}  at {}, total {} \n",
                             (size_t)this,
                             (size_t)stage,
                             node.size());
    return (int)node.size();
}

void
state::run_create()
{
    KRG_check(m_stage == state_stage::create, "Expected proper state!");
    run_items(m_stage);
    m_stage = state_stage::connect;
}

void
state::run_connect()
{
    KRG_check(m_stage == state_stage::connect, "Expected proper state!");
    run_items(m_stage);
    m_stage = state_stage::init;
}

void
state::run_init()
{
    KRG_check(m_stage == state_stage::init, "Expected proper state!");
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

}  // namespace kryga::gs