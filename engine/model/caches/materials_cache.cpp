#include "model/caches/materials_cache.h"

#include "model/object_constructor.h"
#include "model/object_construction_context.h"
#include "vulkan_render/render_loader.h"

#include "core/fs_locator.h"

namespace agea
{
namespace model
{
void
materials_cache::init()
{
    auto assets_path = glob::resource_locator::get()->resource_dir(category::assets);

    auto handler = [this](const std::string& path)
    {
        object_constructor_context occ;
        auto obj = agea::model::object_constructor::class_object_load(path, occ);

        AGEA_check(obj, "should not be empty!");

        auto mobj = cast_ref<material>(occ.extract_last());

        mobj->prepare_for_rendering();

        m_materials[obj->id()] = mobj;

        return true;
    };

    glob::resource_locator::get()->run_over_folder(category::assets, handler, MATERIAL_EXT);
}

std::shared_ptr<agea::model::material>
materials_cache::get(const std::string& id)
{
    auto itr = m_materials.find(id);
    if (itr == m_materials.end())
    {
        return nullptr;
    }

    return itr->second;
}

}  // namespace model
}  // namespace agea
