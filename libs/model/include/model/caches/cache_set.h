#pragma once

#include "model/model_minimal.h"
#include "model/model_fwds.h"

#include <model/caches/caches_map.h>

#include <utils/singleton_instance.h>

namespace agea
{
namespace model
{
class cache_set
{
public:
    cache_set();
    ~cache_set();

    cache_set&
    operator=(cache_set&&) noexcept;
    cache_set(cache_set&&) noexcept;

    std::unique_ptr<objects_cache> objects;
    std::unique_ptr<components_cache> components;
    std::unique_ptr<game_objects_cache> game_objects;
    std::unique_ptr<materials_cache> materials;
    std::unique_ptr<meshes_cache> meshes;
    std::unique_ptr<textures_cache> textures;
    std::unique_ptr<shader_effects_cache> shader_effects;

    std::unique_ptr<caches_map> map;
};

}  // namespace model

namespace glob
{

struct class_objects_cache_set
    : public ::agea::singleton_instance<::agea::model::cache_set, class_objects_cache_set>
{
};

struct objects_cache_set
    : public ::agea::singleton_instance<::agea::model::cache_set, objects_cache_set>
{
};

void
init_global_caches(singleton_registry& r);

}  // namespace glob

}  // namespace agea