#pragma once

#include <ar/ar_defines.h>

#include <error_handling/error_handling.h>
#include <core/reflection/reflection_type.h>

namespace kryga
{
class render_translator;

KRG_ar_render_overrides();
namespace root
{
class smart_object;

result_code
mesh__cmd_builder(reflection::type_context__render_cmd_build& ctx);
result_code
mesh__cmd_destroyer(reflection::type_context__render_cmd_build& ctx);

result_code
texture__cmd_builder(reflection::type_context__render_cmd_build& ctx);
result_code
texture__cmd_destroyer(reflection::type_context__render_cmd_build& ctx);

result_code
sampler__cmd_builder(reflection::type_context__render_cmd_build& ctx);
result_code
sampler__cmd_destroyer(reflection::type_context__render_cmd_build& ctx);

result_code
shader_effect__cmd_builder(reflection::type_context__render_cmd_build& ctx);
result_code
shader_effect__cmd_destroyer(reflection::type_context__render_cmd_build& ctx);

result_code
material__cmd_builder(reflection::type_context__render_cmd_build& ctx);
result_code
material__cmd_destroyer(reflection::type_context__render_cmd_build& ctx);

result_code
game_object_component__cmd_builder(reflection::type_context__render_cmd_build& ctx);
result_code
game_object_component__cmd_destroyer(reflection::type_context__render_cmd_build& ctx);

// Base physics dispatch: recurse into render children so a typed physics handler
// (e.g. terrain) is reached the same way render builds the hierarchy.
result_code
game_object_component__physics_cmd_builder(reflection::type_context__physics_cmd_build& ctx);
result_code
game_object_component__physics_cmd_destroyer(reflection::type_context__physics_cmd_build& ctx);

}  // namespace root
}  // namespace kryga
