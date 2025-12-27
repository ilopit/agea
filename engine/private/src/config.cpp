#include "engine/config.h"

#include <utils/defines_utils.h>
#include <serialization/serialization.h>

#define extract(conf, value, type) conf##.##.value = container[AGEA_stringify(value)].as<type>()
#define extract_id(conf, value) \
    conf##.##.value = utils::id::from(container[AGEA_stringify(value)].as<type>())

namespace agea
{
glob::config::type glob::config::type::s_instance;

namespace editor
{

namespace
{
template <typename T>
void
extract_field(agea::serialization::container& c, const std::string& key, T& field)
{
    auto v = c[key];

    if (!v.IsScalar())
    {
        return;
    }

    if constexpr (std::is_same<T, ::agea::utils::id>::value)
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

    extract_field(container, AGEA_stringify(force_recompile_shaders), force_recompile_shaders);
    extract_field(container, AGEA_stringify(fps_lock), fps_lock);
    extract_field(container, AGEA_stringify(level), level);
    extract_field(container, AGEA_stringify(window_h), window_h);
    extract_field(container, AGEA_stringify(window_w), window_w);
}

}  // namespace editor
}  // namespace agea
