#pragma once

#include "core/model_fwds.h"

#include <core/caches/caches_map.h>

#include <utils/singleton_instance.h>

namespace agea
{
namespace core
{
class cache_set
{
public:
    cache_set();
    ~cache_set();

    cache_set&
    operator=(cache_set&&) noexcept;
    cache_set(cache_set&&) noexcept;

    void
    clear();

    // clang-format off
    objects_cache        objects;
    components_cache     components;
    game_objects_cache   game_objects;
    materials_cache      materials;
    meshes_cache         meshes;
    textures_cache       textures;
    shader_effects_cache shader_effects;
    // clang-format on

    caches_map map;
};

}  // namespace core

}  // namespace agea