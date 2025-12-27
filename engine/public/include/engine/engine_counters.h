#pragma once

#include <cstdint>

#include <utils/singleton_instance.h>
#include <utils/agea_log.h>
#include <utils/clock.h>

namespace agea
{

struct engine_counters
{
    utils::counter<16> consume_updates;
    utils::counter<16> draw;
    utils::counter<16> frame;
    utils::counter<16> input;
    utils::counter<16> tick;
    utils::counter<16> ui_tick;
    utils::counter<16> sync;
};

template <typename T>
struct scope
{
    scope(T& to_update)
        : m_to_update(&to_update)
    {
    }

    ~scope()
    {
        m_to_update->update(utils::get_current_time_mks() - m_start);
    }

    uint64_t m_start = utils::get_current_time_mks();
    T* m_to_update = nullptr;
};

namespace glob
{
struct engine_counters : public singleton_instance<::agea::engine_counters, engine_counters>
{
};
}  // namespace glob

}  // namespace agea

#define AGEA_make_scope(VAR)                                        \
    volatile scope<decltype(::agea::engine_counters::VAR)> scope_s( \
        ::agea::glob::engine_counters::getr().VAR)