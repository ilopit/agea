#include "model/package.h"

#include "utils/agea_log.h"
#include "model/object_constructor.h"
#include "model/object_construction_context.h"
#include "model/caches/base_cache.h"

#include <filesystem>

namespace agea
{
namespace model
{

namespace
{

const std::vector<architype> k_enums_to_handle{architype::texture, architype::material,
                                               architype::mesh, architype::component,
                                               architype::game_object};

std::string
get_name(architype id)
{
    switch (id)
    {
    case agea::model::architype::texture:
        return "textures";
    case agea::model::architype::mesh:
        return "meshes";
    case agea::model::architype::material:
        return "materials";
    case agea::model::architype::game_object:
        return "game_objects";
    case agea::model::architype::component:
        return "components";
    default:
        break;
    }
    AGEA_never("Uncovered!");
    return "";
}

}  // namespace

package::package(package&&) noexcept = default;
package&
package::operator=(package&&) noexcept = default;

package::package()
    : m_occ(nullptr)
{
}

package::~package()
{
}
bool
package::load_package(const utils::path& path, package& p, cache_set_ref global_cs)
{
    ALOG_INFO("Loading package {0}", path.str());

    p.m_global_cs = global_cs;

    p.m_path = path;
    p.m_id = path.fs().filename().generic_string();
    p.m_occ = std::make_unique<object_constructor_context>(p.m_global_cs, p.m_local_cs.get_ref(),
                                                           &p.m_objects);
    p.m_occ->m_path_prefix = utils::path(p.m_path);

    for (auto id : k_enums_to_handle)
    {
        if (!load_package_conteiners(id, p))
        {
            ALOG_LAZY_ERROR;
            return false;
        }
    }

    return true;
}

bool
package::save_package(const utils::path& path, const package& p)
{
    ALOG_INFO("Loading package {0}", path.str());

    p.set_path(path);

    if (!path.exists())
    {
        ALOG_INFO("There is no [{0}] conainer in [{1}] package!", p.get_id());
        std::filesystem::create_directories(path.fs());
    }

    for (auto& o : p.m_objects)
    {
        auto obj_path = p.get_resource_path(utils::path(get_name(o->get_architype_id())));
        if (!obj_path.exists())
        {
            std::filesystem::create_directories(obj_path.fs());
        }

        obj_path.append(o->get_id() + ".aobj");

        if (!object_constructor::class_object_save(*o, path))
        {
            ALOG_LAZY_ERROR;
            return false;
        }
    }

    return true;
}

bool
package::load_package_conteiners(architype id, package& p)
{
    auto module_path = p.get_resource_path(get_name(id));

    if (!module_path.exists())
    {
        ALOG_WARN("There is no [{0}] conainer in [{1}] package!", get_name(id), p.get_id());
        return true;
    }

    for (auto& resource : std::filesystem::directory_iterator(module_path.fs()))
    {
        if (resource.is_directory())
        {
            continue;
        }

        auto package_path = get_name(id) / resource.path().filename();

        auto obj =
            model::object_constructor::class_object_load(utils::path(package_path), *p.m_occ);

        if (!obj)
        {
            return false;
        }
    }

    return true;
}

void
package::propagate_to_global_caches()
{
    for (auto& o : m_objects)
    {
        o->set_package(this);
    }

    for (auto& c : m_local_cs.map->get_items())
    {
        for (auto& i : c.second->get_items())
        {
            m_global_cs.map->get_cache(c.first).add_item(*i.second);
        }
    }
}

}  // namespace model
}  // namespace agea