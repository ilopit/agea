#pragma once

namespace agea
{
namespace render
{
enum class depth_stencil_mode
{
    none = 0,
    stencil,
    outline
};

enum class alpha_mode
{
    none = 0,
    world,
    ui
};

}  // namespace render
}  // namespace agea