#include "global_state/global_state.h"

#include <utils/kryga_log.h>

#include <array>
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

namespace
{
using stage_actions = std::array<std::vector<::kryga::gs::scheduled_action>,
                                 (size_t)::kryga::gs::state::state_stage::number_of_stages>;

stage_actions&
persistent_actions()
{
    static stage_actions g;
    return g;
}
}  // namespace

int
persistent_schedule::add(::kryga::gs::state::state_stage stage,
                         ::kryga::gs::scheduled_action action)
{
    auto& bucket = persistent_actions()[(size_t)stage];
    bucket.push_back(std::move(action));
    return (int)bucket.size();
}

void
persistent_schedule::run(::kryga::gs::state::state_stage stage, ::kryga::gs::state& s)
{
    for (auto& a : persistent_actions()[(size_t)stage])
    {
        a(s);
    }
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
        // Clear scheduled actions - static initializers may have added actions
        // that shouldn't persist across state resets
        for (auto& actions : m_scheduled_actions)
        {
            actions.clear();
        }
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
    // Persistent (static-init) actions fire first every time — they register
    // subsystems that must survive glob_state_reset (root/base packages etc).
    ::kryga::glob::persistent_schedule::run(stage, *this);

    auto& node = m_scheduled_actions[(size_t)stage];
    for (auto& a : node)
    {
        a(*this);
    }

    node.clear();
}

}  // namespace kryga::gs