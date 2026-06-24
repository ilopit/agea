#pragma once

#include <ar/ar_defines.h>

#include <error_handling/error_handling.h>
#include <core/reflection/reflection_type.h>

// The generated render-package glue (package.ui.render.ar.cpp) emits the package
// connect() body which references glob::glob_state() / gs::state. Other packages
// pull global_state in transitively via their gpu_types material headers; this
// package has no materials, so include it explicitly.
#include <global_state/global_state.h>

namespace kryga
{
namespace root
{
class smart_object;
}

KRG_ar_render_overrides();

result_code
ui_panel__cmd_builder(reflection::type_context__render_cmd_build& ctx);
result_code
ui_panel__cmd_destroyer(reflection::type_context__render_cmd_build& ctx);

result_code
ui_text__cmd_builder(reflection::type_context__render_cmd_build& ctx);
result_code
ui_text__cmd_destroyer(reflection::type_context__render_cmd_build& ctx);

}  // namespace kryga
