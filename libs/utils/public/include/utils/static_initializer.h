#pragma once

#include <functional>

namespace agea
{
using si_action = std::function<void()>;

class static_initializer
{
public:
    static size_t
    schedule(si_action action)
    {
        instance().m_actions.push_back(std::move(action));

        return instance().m_actions.size();
    }

    static void
    init()
    {
        for (auto& a : instance().m_actions)
        {
            a();
        }

        instance().m_actions.clear();
    }

private:
    static static_initializer&
    instance()
    {
        static static_initializer s_instance;
        return s_instance;
    }

    static_initializer() = default;

    std::vector<std::function<void()>> m_actions;
};

}  // namespace agea

#define AGEA_schedule_static_init(action) \
    const static auto si_identifier##__COUNTER__ = ::agea::static_initializer::schedule(action) \
    \