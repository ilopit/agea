#pragma once

#include <game/game_system.h>

#include <utils/check.h>

#include <array>
#include <memory>
#include <vector>

namespace kryga
{
namespace gs
{
class state;
}

namespace game
{

class game_system_manager
{
public:
    template <typename T, typename... Args>
    T&
    add(Args&&... args)
    {
        auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
        auto& ref = *ptr;
        register_system(std::move(ptr));
        return ref;
    }

    template <typename T>
    T*
    find() const
    {
        for (auto& sys : m_systems)
        {
            if (auto* p = dynamic_cast<T*>(sys.get()))
            {
                return p;
            }
        }
        return nullptr;
    }

    void
    register_system(std::unique_ptr<game_system> sys);

    void
    tick_phase(game_phase phase, float dt);

    void
    on_begin_play();

    void
    on_end_play();

    size_t
    system_count() const
    {
        return m_systems.size();
    }

private:
    static constexpr size_t k_phase_count = static_cast<size_t>(game_phase::count_);

    std::vector<std::unique_ptr<game_system>> m_systems;
    std::array<std::vector<game_system*>, k_phase_count> m_by_phase;
};

}  // namespace game
}  // namespace kryga
