#pragma once

#include <string>

#include "model/architype.h"
#include <functional>

namespace agea
{
namespace root
{
class asset;
class component;
class game_object;
class game_object_component;
class texture;
class mesh;
class material;
class smart_object;
class shader_effect;
class light_component;

}  // namespace root
namespace core
{

class level;
class package;
class package_manager;
class object_load_context;
class caches_map;
class cache_set;
class cache_set_ref;
class class_objects_cache;

template <typename T, architype ID>
class cache;

using objects_cache = cache<root::smart_object, architype::smart_object>;
using components_cache = cache<root::component, architype::component>;
using game_objects_cache = cache<root::game_object, architype::game_object>;
using textures_cache = cache<root::texture, architype::texture>;
using meshes_cache = cache<root::mesh, architype::mesh>;
using materials_cache = cache<root::material, architype::material>;
using shader_effects_cache = cache<root::shader_effect, architype::shader_effect>;

}  // namespace core
}  // namespace agea