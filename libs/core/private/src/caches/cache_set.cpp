#pragma once

#include "core/caches/cache_set.h"

#include "core/caches/caches_map.h"

#include "core/model_fwds.h"

namespace agea
{

namespace core
{
cache_set::cache_set()
    : objects(std::make_unique<objects_cache>())
    , components(std::make_unique<components_cache>())
    , game_objects(std::make_unique<game_objects_cache>())
    , materials(std::make_unique<materials_cache>())
    , meshes(std::make_unique<meshes_cache>())
    , textures(std::make_unique<textures_cache>())
    , shader_effects(std::make_unique<shader_effects_cache>())
    , map(std::make_unique<caches_map>())
{
    map->add_cache(objects.get());
    map->add_cache(components.get());
    map->add_cache(game_objects.get());
    map->add_cache(materials.get());
    map->add_cache(meshes.get());
    map->add_cache(textures.get());
    map->add_cache(shader_effects.get());
}

cache_set::cache_set(cache_set&&) noexcept = default;

void
cache_set::clear()
{
    objects->clear();
    components->clear();
    game_objects->clear();
    materials->clear();
    meshes->clear();
    textures->clear();
    shader_effects->clear();
}

cache_set&
cache_set::operator=(cache_set&&) noexcept = default;

cache_set::~cache_set() = default;

}  // namespace core

}  // namespace agea