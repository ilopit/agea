#pragma once

#include "model/caches/cache_set.h"

#include "model/caches/components_cache.h"
#include "model/caches/game_objects_cache.h"
#include "model/caches/materials_cache.h"
#include "model/caches/meshes_cache.h"
#include "model/caches/textures_cache.h"

#include "model/model_fwds.h"

namespace agea
{
namespace model
{

cache_set::cache_set()
    : objects(std::make_unique<objects_cache>())
    , components(std::make_unique<components_cache>())
    , game_objects(std::make_unique<game_objects_cache>())
    , materials(std::make_unique<materials_cache>())
    , meshes(std::make_unique<meshes_cache>())
    , textures(std::make_unique<textures_cache>())
    , map(std::make_unique<caches_map>())
{
    map->add_cache(objects.get());
    map->add_cache(components.get());
    map->add_cache(game_objects.get());
    map->add_cache(materials.get());
    map->add_cache(meshes.get());
    map->add_cache(textures.get());
}

cache_set::cache_set(cache_set&&) noexcept = default;

cache_set_ref
cache_set::get_ref()
{
    cache_set_ref csf;

    csf.objects = objects.get();
    csf.components = components.get();
    csf.game_objects = game_objects.get();
    csf.materials = materials.get();
    csf.meshes = meshes.get();
    csf.textures = textures.get();
    csf.map = map.get();

    return csf;
}

cache_set&
cache_set::operator=(cache_set&&) noexcept = default;

cache_set::~cache_set() = default;

}  // namespace model
}  // namespace agea