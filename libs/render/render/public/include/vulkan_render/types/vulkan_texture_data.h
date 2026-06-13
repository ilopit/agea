#pragma once

#include "vulkan_render/types/vulkan_render_resource.h"
#include "vulkan_render/utils/vulkan_image.h"

#include <render_types/render_handle.h>

#include <utils/id.h>

namespace kryga
{
namespace render
{
enum class texture_format : uint32_t
{
    unknown = 0,
    rgba8,
    rgba16f,
    r11g11b10f
};

// Texture data stored by value in the loader's bindless slot_storage.
// The slot() from vulkan_render_resource serves as the bindless index.
class texture_data : public vulkan_render_resource
{
public:
    texture_data() = default;

    texture_data(const ::kryga::utils::id& id, gpu_data_index_type idx)
        : vulkan_render_resource(id, idx)
    {
    }

    // Bindless index is the slot index in the bindless texture storage
    uint32_t
    get_bindless_index() const
    {
        return slot();
    }

    vk_utils::vulkan_image_sptr image;
    vk_utils::vulkan_image_view_sptr image_view;
    texture_format format = texture_format::unknown;
    // The bindless-pool handle this texture occupies (set by alloc_texture).
    // Release goes through it (reclaim needs index + generation), so owners
    // hold the texture_data* and never need an id lookup.
    render::types::texture_handle handle;
};

}  // namespace render
}  // namespace kryga
