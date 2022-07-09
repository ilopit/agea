#include "model/conteiner_loader.h"

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
                                          conteiner_loader::obj_loader loader,
                                          const utils::path& folder_path,
                                          object_constructor_context& occ)
{
    auto module_path = folder_path;

    auto conteiner_name = get_name(id);

    module_path.append(conteiner_name);

    if (!module_path.exists())
    {
        ALOG_INFO("Conteiner {0} doesn't exists at {1}", conteiner_name, folder_path.str());
        return true;
    }

    for (auto& resource : std::filesystem::directory_iterator(module_path.fs()))
    {
        if (resource.is_directory())
        {
            continue;
        }

        auto obj = loader(utils::path(resource), occ);

        if (!obj)
        {
            return false;
        }
    }

    return true;
}

bool
conteiner_loader::save_objects_conteiners(architype id,
                                          obj_saver s,
                                          const utils::path& folder_path,
                                          cache_set_ref obj_set)
{
    auto conteiner_name = get_name(id);

    auto module_path = folder_path / conteiner_name;

    if (module_path.exists())
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    auto& items = obj_set.map->get_cache(id).get_items();

    if (!items.empty())
    {
        std::error_code ec;
        std::filesystem::create_directories(module_path.fs(), ec);
    }

    for (auto& i : items)
    {
        auto& obj = *i.second;

        auto file_name = obj.get_id().str() + ".aobj";
        auto full_path = module_path / file_name;

        auto result = s(obj, full_path);

        if (!result)
        {
            ALOG_LAZY_ERROR;
            return false;
        }
    }

    return true;
}

}  // namespace model
}  // namespace agea