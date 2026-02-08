#include "engine/config.h"

#include <global_state/global_state.h>
#include <utils/defines_utils.h>
#include <serialization/serialization.h>

namespace kryga
{
void
state_mutator__config::set(gs::state& s)
{
    auto p = s.create_box<editor::config>("config");
    s.m_config = p;
}

namespace editor
{

namespace
{
template <typename T>
void
extract_field(kryga::serialization::container& c, const std::string& key, T& field)
{
    auto v = c[key];

    if (!v.IsScalar())
    {
        return;
    }

    if constexpr (std::is_same<T, ::kryga::utils::id>::value)
    {
        field = AID(v.as<std::string>());
    }
    else
    {
        field = v.as<T>();
    }
}
}  // namespace

void
config::load(const utils::path& config_path)
{
    serialization::container container;
    if (!serialization::read_container(config_path, container))
    {
        return;
    }

    extract_field(container, KRG_stringify(force_recompile_shaders), force_recompile_shaders);
    extract_field(container, KRG_stringify(fps_lock), fps_lock);
    extract_field(container, KRG_stringify(level), level);
    extract_field(container, KRG_stringify(window_h), window_h);
    extract_field(container, KRG_stringify(window_w), window_w);
}

}  // namespace editor
}  // namespace kryga
