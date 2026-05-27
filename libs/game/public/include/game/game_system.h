#pragma once

#include <global_state/system.h>

namespace kryga::game
{

enum class game_phase
{
    pre_physics,
    post_physics,
    late_update,

    count_
};

class game_system : public gs::system
{
public:
    std::span<const std::string_view>
    deps() const override
    {
        return {};
    }

    virtual game_phase
    phase() const = 0;

    virtual void
    tick(float dt) = 0;

    virtual void
    on_begin_play()
    {
    }

    virtual void
    on_end_play()
    {
    }

    bool
    is_enabled() const
    {
        return m_enabled;
    }

    void
    set_enabled(bool v)
    {
        m_enabled = v;
    }

private:
    bool m_enabled = true;
};

}  // namespace kryga::game
