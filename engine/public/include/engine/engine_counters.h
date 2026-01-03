#pragma once

#include <cstdint>

#include <utils/singleton_instance.h>
#include <utils/kryga_log.h>
#include <utils/clock.h>

namespace kryga
{

struct engine_counters
{
    utils::counter<24> consume_updates;
    utils::counter<24> draw;
    utils::counter<24> frame;
    utils::counter<24> input;
    utils::counter<24> tick;
    utils::counter<24> ui_tick;
    utils::counter<24> sync;
    utils::counter<24> objects;
    utils::counter<24> culled_draws;
    utils::counter<24> all_draws;
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
struct engine_counters : public singleton_instance<::kryga::engine_counters, engine_counters>
{
};
}  // namespace glob

}  // namespace kryga

#define KRG_make_scope(VAR)                                        \
    volatile scope<decltype(::kryga::engine_counters::VAR)> scope_s( \
        ::kryga::glob::engine_counters::getr().VAR)