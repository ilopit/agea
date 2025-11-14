#pragma once

#include "core/caches/cache_set.h"

#include "core/caches/caches_map.h"

#include "core/model_fwds.h"

namespace agea
{

namespace core
{
cache_set::cache_set()
{
    map.add_cache(&objects);
    map.add_cache(&components);
    map.add_cache(&game_objects);
    map.add_cache(&materials);
    map.add_cache(&meshes);
    map.add_cache(&textures);
    map.add_cache(&shader_effects);
}

cache_set::cache_set(cache_set&&) noexcept = default;

void
cache_set::clear()
{
    objects.clear();
    components.clear();
    game_objects.clear();
    materials.clear();
    meshes.clear();
    textures.clear();
    shader_effects.clear();
}

cache_set&
cache_set::operator=(cache_set&&) noexcept = default;

cache_set::~cache_set() = default;

}  // namespace core

}  // namespace agea