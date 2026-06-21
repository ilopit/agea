#include <game_session/player_state.h>

#include <serialization/serialization.h>

namespace kryga::game
{

void
player_state::to_container(serialization::container& c) const
{
    c["current_level"] = current_level;
    c["health"] = health;

    serialization::container pos;
    pos.SetStyle(YAML::EmitterStyle::Flow);
    pos.push_back(position[0]);
    pos.push_back(position[1]);
    pos.push_back(position[2]);
    c["position"] = pos;

    serialization::container flags_c;
    for (const auto& [k, v] : flags)
    {
        flags_c[k] = v;
    }
    c["flags"] = flags_c;
}

bool
player_state::from_container(const serialization::container& c)
{
    if (!c.IsMap())
    {
        return false;
    }

    if (c["current_level"])
    {
        current_level = c["current_level"].as<std::string>();
    }

    flags.clear();
    if (c["flags"] && c["flags"].IsMap())
    {
        for (const auto& kv : c["flags"])
        {
            flags[kv.first.as<std::string>()] = kv.second.as<int64_t>();
        }
    }
    return true;
}

}  // namespace kryga::game
