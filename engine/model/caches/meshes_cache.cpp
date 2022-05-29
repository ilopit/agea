#include "model/caches/meshes_cache.h"

#include "model/object_constructor.h"
#include "model/object_construction_context.h"

#include "core/fs_locator.h"

namespace agea
{
namespace model
{
void
meshes_cache::init()
{
    auto assets_path = glob::resource_locator::get()->resource_dir(category::assets);

    auto handler = [this](const std::string& path)
    {
        object_constructor_context occ;
        auto obj = agea::model::object_constructor::class_object_load(path, occ);

        AGEA_check(obj, "should not be empty!");

        auto mobj = cast_ref<mesh>(occ.extract_last());
        mobj->prepare_for_rendering();

        m_meshes[obj->get_id()] = mobj;

        return true;
    };

    glob::resource_locator::get()->run_over_folder(category::assets, handler, MESH_EXT);
}

std::shared_ptr<agea::model::mesh>
meshes_cache::get(const std::string& id)
{
    return m_meshes.at(id);
}

}  // namespace model
}  // namespace agea
