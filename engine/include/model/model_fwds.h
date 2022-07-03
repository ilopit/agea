#pragma once

#include <string>

#include "model/architype.h"
#include <functional>

namespace agea
{
namespace model
{
class caches_map;
class cache_set;
class cache_set_ref;
class class_objects_cache;
class component;
class game_object;
class game_object_component;
class object_constructor_context;
class level;
class package;
class package_manager;
class texture;
class mesh;
class material;
class smart_object;

template <typename T, architype ID>
class cache;

using objects_cache = cache<smart_object, architype::smart_object>;
using components_cache = cache<component, architype::component>;
using game_objects_cache = cache<game_object, architype::game_object>;
using textures_cache = cache<texture, architype::texture>;
using meshes_cache = cache<mesh, architype::mesh>;
using materials_cache = cache<material, architype::material>;

}  // namespace model
}  // namespace agea