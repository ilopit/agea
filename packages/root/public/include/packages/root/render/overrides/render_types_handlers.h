#pragma once

#include <ar/ar_defines.h>

#include <error_handling/error_handling.h>
#include <core/reflection/reflection_type.h>

namespace kryga
{
class render_bridge;

KRG_ar_render_overrides();
namespace root
{
class smart_object;

result_code
mesh__render_loader(reflection::type_context__render& ctx);
result_code
mesh__render_destructor(reflection::type_context__render& ctx);

result_code
material__render_loader(reflection::type_context__render& ctx);
result_code
material__render_destructor(reflection::type_context__render& ctx);

result_code
texture__render_loader(reflection::type_context__render& ctx);
result_code
texture__render_destructor(reflection::type_context__render& ctx);

result_code
sampler__render_loader(reflection::type_context__render& ctx);
result_code
sampler__render_destructor(reflection::type_context__render& ctx);

result_code
game_object_component__render_loader(reflection::type_context__render& ctx);
result_code
game_object_component__render_destructor(reflection::type_context__render& ctx);

result_code
shader_effect__render_loader(reflection::type_context__render& ctx);
result_code
shader_effect__render_destructor(reflection::type_context__render& ctx);

}  // namespace root
}  // namespace kryga
