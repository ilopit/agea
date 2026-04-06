#pragma once

#include <gpu_types/gpu_shadow_types.h>

#include <cstdint>

namespace kryga
{
namespace render
{

enum class render_mode : uint32_t
{
    instanced,  // Batched instanced drawing with GPU cluster culling
    per_object  // Legacy per-object drawing with CPU light grid
};

enum class pcf_mode : uint32_t
{
    pcf_3x3 = KGPU_PCF_3X3,
    pcf_5x5 = KGPU_PCF_5X5,
    pcf_7x7 = KGPU_PCF_7X7,
    poisson16 = KGPU_PCF_POISSON16,
    poisson32 = KGPU_PCF_POISSON32,
};

}  // namespace render
}  // namespace kryga
