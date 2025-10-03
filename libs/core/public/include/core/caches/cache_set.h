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

    std::unique_ptr<objects_cache> objects;
    std::unique_ptr<components_cache> components;
    std::unique_ptr<game_objects_cache> game_objects;
    std::unique_ptr<materials_cache> materials;
    std::unique_ptr<meshes_cache> meshes;
    std::unique_ptr<textures_cache> textures;
    std::unique_ptr<shader_effects_cache> shader_effects;

    std::unique_ptr<caches_map> map;
};

}  // namespace core

}  // namespace agea