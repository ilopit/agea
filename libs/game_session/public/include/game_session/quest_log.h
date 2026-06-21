#pragma once

#include <serialization/serialization_fwds.h>

#include <utils/id.h>

#include <unordered_map>

namespace kryga::game
{

enum class quest_state
{
    inactive = 0,
    active,
    complete
};

// Minimal quest framework: tracks per-quest state. Concrete quest logic is
// expected to drive transitions via start()/advance(); poll() is the per-tick
// extension point (empty for now). Serializes alongside player_state.
class quest_log
{
public:
    quest_state
    state_of(const utils::id& quest) const;

    // inactive -> active
    void
    start(const utils::id& quest);

    // Explicit transition to any state.
    void
    advance(const utils::id& quest, quest_state to);

    bool
    is_complete(const utils::id& quest) const
    {
        return state_of(quest) == quest_state::complete;
    }

    // Per-frame hook. No built-in conditions yet — override point for
    // condition-driven quests later.
    void
    poll(float dt);

    void
    clear();

    void
    to_container(serialization::container& c) const;

    bool
    from_container(const serialization::container& c);

private:
    std::unordered_map<utils::id, quest_state> m_quests;
};

}  // namespace kryga::game
