#include "engine/config.h"

#include <utils/defines_utils.h>
#include <serialization/serialization.h>

#define extract(conf, value, type) conf##.##.value = conteiner[AGEA_stringify(value)].as<type>()
#define extract_id(conf, value) \
    conf##.##.value = utils::id::from(conteiner[AGEA_stringify(value)].as<type>())

namespace agea
{
namespace editor
{

namespace
{
template <typename T>
void
extract_field(agea::serialization::conteiner& c, const std::string& key, T& field)
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
    serialization::conteiner conteiner;
    if (!serialization::read_container(config_path, conteiner))
    {
        return;
    }

    extract_field(conteiner, AGEA_stringify(force_recompile_shaders), force_recompile_shaders);
    extract_field(conteiner, AGEA_stringify(fps_lock), fps_lock);
    extract_field(conteiner, AGEA_stringify(level), level);
    extract_field(conteiner, AGEA_stringify(window_h), window_h);
    extract_field(conteiner, AGEA_stringify(window_w), window_w);
}

}  // namespace editor
}  // namespace agea
