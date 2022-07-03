#include "model/objects_loader.h"

#include "utils/agea_log.h"
#include "model/object_constructor.h"
#include "model/object_construction_context.h"
#include "model/caches/hash_cache.h"

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

bool
conteiner_loader::load_objects_conteiners(architype id,
                                          bool is_class,
                                          const utils::path& folder_path,
                                          object_constructor_context& occ)
{
    std::string prefix =
        (is_class ? std::string("class/") : std::string("instance/")) + get_name(id);

    auto module_path = folder_path;
    module_path.append(prefix);

    if (!module_path.exists())
    {
        return true;
    }

    for (auto& resource : std::filesystem::directory_iterator(module_path.fs()))
    {
        if (resource.is_directory())
        {
            continue;
        }

        auto obj =
            is_class ? model::object_constructor::class_object_load(utils::path(resource), occ)
                     : model::object_constructor::instance_object_load(utils::path(resource), occ);

        if (!obj)
        {
            return false;
        }
    }

    return true;
}

bool
conteiner_loader::save_objects_conteiners(architype id,
                                          bool is_class,
                                          const utils::path& folder_path,
                                          cache_set_ref class_objects_cs_ref,
                                          cache_set_ref objects_cs_ref)
{
    std::string prefix =
        (is_class ? std::string("class/") : std::string("instance/")) + get_name(id);

    auto module_path = folder_path;
    module_path.append(prefix);

    if (module_path.exists())
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    auto from = is_class ? class_objects_cs_ref : objects_cs_ref;

    auto& items = from.map->get_cache(id).get_items();

    if (!items.empty())
    {
        std::error_code ec;
        std::filesystem::create_directories(module_path.fs(), ec);
    }

    for (auto& i : items)
    {
        auto file_name = i.second->get_id().str() + ".aobj";
        auto full_path = module_path;
        full_path.append(file_name);

        auto obj = is_class ? model::object_constructor::class_object_save(*i.second, full_path)
                            : model::object_constructor::instance_object_save(*i.second, full_path);

        if (!obj)
        {
            ALOG_LAZY_ERROR;
            return false;
        }
    }

    return true;
}

}  // namespace model
}  // namespace agea