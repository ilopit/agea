#include "model/caches/textures_cache.h"

#include "model/object_constructor.h"
#include "model/object_construction_context.h"

#include "core/fs_locator.h"

namespace agea
{
namespace model
{
void
textures_cache::init()
{
    auto assets_path = glob::resource_locator::get()->resource_dir(category::assets);

    auto handler = [this](const std::string& path)
    {
        object_constructor_context occ;
        auto obj = agea::model::object_constructor::class_object_load(path, occ);

        AGEA_check(obj, "should not be empty!");

        auto tobj = cast_ref<model::texture>(occ.extract_last());

        tobj->prepare_for_rendering();

        m_textures[obj->get_id()] = tobj;

        return true;
    };

    glob::resource_locator::get()->run_over_folder(category::assets, handler, TEXTURE_EXT);
}

std::shared_ptr<agea::model::texture>
textures_cache::get(const std::string& id)
{
    return m_textures.at(id);
}

}  // namespace model
}  // namespace agea
