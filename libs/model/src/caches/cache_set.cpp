#pragma once

#include "model/caches/cache_set.h"

#include "model/caches/components_cache.h"
#include "model/caches/game_objects_cache.h"
#include "model/caches/materials_cache.h"
#include "model/caches/meshes_cache.h"
#include "model/caches/textures_cache.h"
#include "model/caches/objects_cache.h"
#include "model/caches/shader_effects_cache.h"
#include "model/caches/empty_objects_cache.h"

#include "model/caches/caches_map.h"

#include "model/model_fwds.h"

namespace agea
{

glob::class_objects_cache_set::type glob::class_objects_cache_set::type::s_instance;
glob::objects_cache_set::type glob::objects_cache_set::type::s_instance;

namespace model
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

cache_set&
cache_set::operator=(cache_set&&) noexcept = default;

cache_set::~cache_set() = default;

}  // namespace model

void
glob::init_global_caches(singleton_registry& r)
{
    // clang-format off
    glob::empty_objects_cache::create(r);

    glob::class_objects_cache_set::create(r);
    glob::class_object_caches_map::create_ref(glob::class_objects_cache_set::get()->map.get());

    glob::class_materials_cache::create_ref(glob::class_objects_cache_set::getr().materials.get());
    glob::class_meshes_cache::create_ref(glob::class_objects_cache_set::getr().meshes.get());
    glob::class_textures_cache::create_ref(glob::class_objects_cache_set::getr().textures.get());
    glob::class_objects_cache::create_ref(glob::class_objects_cache_set::getr().objects.get());
    glob::class_components_cache::create_ref(glob::class_objects_cache_set::getr().components.get());
    glob::class_shader_effects_cache::create_ref(glob::class_objects_cache_set::getr().shader_effects.get());

    glob::objects_cache_set::create(r);
    glob::object_caches_map::create_ref(glob::objects_cache_set::get()->map.get());

    glob::materials_cache::create_ref(glob::objects_cache_set::getr().materials.get());
    glob::meshes_cache::create_ref(glob::objects_cache_set::getr().meshes.get());
    glob::textures_cache::create_ref(glob::objects_cache_set::getr().textures.get());
    glob::objects_cache::create_ref(glob::objects_cache_set::getr().objects.get());
    glob::components_cache::create_ref(glob::objects_cache_set::getr().components.get());
    glob::shader_effects_cache::create_ref(glob::objects_cache_set::getr().shader_effects.get());
    // clang-format on
}

}  // namespace agea