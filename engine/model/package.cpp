#include "model/package.h"

#include "utils/agea_log.h"
#include "model/object_constructor.h"
#include "model/object_construction_context.h"
#include "model/caches/hash_cache.h"
#include "model/conteiner_loader.h"

#include "model/rendering/texture.h"
#include "model/rendering/material.h"
#include "model/rendering/mesh.h"

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
package::load_package(const utils::path& path,
                      package& p,
                      cache_set_ref class_global_set,
                      cache_set_ref instance_global_set)
{
    ALOG_INFO("Loading package {0}", path.str());

    std::string name, extension;
    path.parse_file_name_and_ext(name, extension);

    if (name.empty() || extension.empty() || extension != "apkg")
    {
        ALOG_ERROR("Loading package failed, {0} {1}", name, extension);
        return false;
    }

    p.m_class_global_set = class_global_set;
    p.m_instance_global_set = instance_global_set;
    p.m_load_path = path;
    p.m_id = core::id::from(name);

    p.m_occ = std::make_unique<object_constructor_context>(
        p.m_load_path, p.m_class_global_set, p.m_class_local_set.get_ref(), p.m_instance_global_set,
        p.m_instance_local_set.get_ref(), &p.m_objects);

    auto class_path = path / "class";
    auto instances_path = path / "instance";

    for (auto id : k_enums_to_handle)
    {
        if (!conteiner_loader::load_objects_conteiners(id, object_constructor::class_object_load,
                                                       class_path, *p.m_occ))
        {
            ALOG_LAZY_ERROR;
            return false;
        }

        if (!conteiner_loader::load_objects_conteiners(id, object_constructor::instance_object_load,
                                                       instances_path, *p.m_occ))
        {
            ALOG_LAZY_ERROR;
            return false;
        }
    }

    for (auto& o : p.m_objects.get_items())
    {
        o->set_package(&p);
    }

    return true;
}

bool
package::save_package(const utils::path& root_folder, const package& p)
{
    ALOG_INFO("Loading package {0}", root_folder.str());

    p.set_save_root_path(root_folder);

    if (!root_folder.exists())
    {
        ALOG_INFO("There is no [{0}] conainer in [{1}] package!", p.get_id().cstr());
        std::filesystem::create_directories(root_folder.fs());
    }
    std::string name = p.get_id().str() + ".apkg";
    auto full_path = root_folder / name;

    auto class_path = full_path / "class";
    auto instances_path = full_path / "instance";

    for (auto id : k_enums_to_handle)
    {
        if (!conteiner_loader::save_objects_conteiners(id, object_constructor::class_object_save,
                                                       class_path, p.m_class_local_set.get_ref()))
        {
            ALOG_LAZY_ERROR;
            return false;
        }

        if (!conteiner_loader::save_objects_conteiners(id, object_constructor::instance_object_save,
                                                       instances_path,
                                                       p.m_instance_local_set.get_ref()))
        {
            ALOG_LAZY_ERROR;
            return false;
        }
    }

    return true;
}

void
package::propagate_to_global_caches()
{
    for (auto& c : m_class_local_set.map->get_items())
    {
        for (auto& i : c.second->get_items())
        {
            m_class_global_set.map->get_cache(c.first).add_item(*i.second);
        }
    }

    for (auto& c : m_instance_local_set.map->get_items())
    {
        for (auto& i : c.second->get_items())
        {
            m_instance_global_set.map->get_cache(c.first).add_item(*i.second);
        }
    }
}

bool
package::prepare_for_rendering()
{
    auto result = m_class_local_set.textures->call_on_items(
        [](model::texture* t)
        {
            if (!t->prepare_for_rendering())
            {
                ALOG_LAZY_ERROR;
                return false;
            }

            return true;
        });

    if (!result)
    {
        return result;
    }

    result = m_class_local_set.materials->call_on_items(
        [](model::material* t)
        {
            if (!t->prepare_for_rendering())
            {
                ALOG_LAZY_ERROR;
                return false;
            }
            return true;
        });

    if (!result)
    {
        return result;
    }

    result = m_class_local_set.meshes->call_on_items(
        [](model::mesh* t)
        {
            if (!t->prepare_for_rendering())
            {
                ALOG_LAZY_ERROR;
                return false;
            }

            return true;
        });

    result = m_instance_local_set.textures->call_on_items(
        [](model::texture* t)
        {
            if (!t->prepare_for_rendering())
            {
                ALOG_LAZY_ERROR;
                return false;
            }

            return true;
        });

    if (!result)
    {
        return result;
    }

    result = m_instance_local_set.materials->call_on_items(
        [](model::material* t)
        {
            if (!t->prepare_for_rendering())
            {
                ALOG_LAZY_ERROR;
                return false;
            }
            return true;
        });

    if (!result)
    {
        return result;
    }

    result = m_instance_local_set.meshes->call_on_items(
        [](model::mesh* t)
        {
            if (!t->prepare_for_rendering())
            {
                ALOG_LAZY_ERROR;
                return false;
            }

            return true;
        });

    return result;
}

}  // namespace model
}  // namespace agea