#include <game_session/quest_log.h>

#include <serialization/serialization.h>

namespace kryga::game
{

quest_state
quest_log::state_of(const utils::id& quest) const
{
    auto it = m_quests.find(quest);
    return it == m_quests.end() ? quest_state::inactive : it->second;
}

void
quest_log::start(const utils::id& quest)
{
    m_quests[quest] = quest_state::active;
}

void
quest_log::advance(const utils::id& quest, quest_state to)
{
    m_quests[quest] = to;
}

void
quest_log::poll(float)
{
    // No built-in conditions yet. Extension point for condition/event-driven
    // quests — intentionally empty in this first cut.
}

void
quest_log::clear()
{
    m_quests.clear();
}

void
quest_log::to_container(serialization::container& c) const
{
    for (const auto& [id, state] : m_quests)
    {
        c[id.str()] = static_cast<int>(state);
    }
}

bool
quest_log::from_container(const serialization::container& c)
{
    m_quests.clear();
    if (!c.IsMap())
    {
        return false;
    }
    for (const auto& kv : c)
    {
        auto id = utils::id::make_id(kv.first.as<std::string>());
        m_quests[id] = static_cast<quest_state>(kv.second.as<int>());
    }
    return true;
}

}  // namespace kryga::game
