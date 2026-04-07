#pragma once

#include <utils/id.h>
#include <utils/buffer.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace kryga
{
namespace render
{

// Represents a sub-region within a lightmap atlas texture
struct atlas_region
{
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};

// Manages packing of multiple mesh lightmaps into shared atlas textures.
// Each mesh gets an atlas_region; UV2 coordinates are remapped from per-mesh [0,1]
// to the atlas sub-region.
class lightmap_atlas
{
public:
    explicit lightmap_atlas(uint32_t width = 2048, uint32_t height = 2048);

    uint32_t
    get_width() const
    {
        return m_width;
    }

    uint32_t
    get_height() const
    {
        return m_height;
    }

    // Allocate a region for a mesh's lightmap.
    // Returns true if the region was successfully packed.
    bool
    allocate(const utils::id& mesh_id, uint32_t region_width, uint32_t region_height);

    // Get the allocated region for a mesh.
    // Returns nullptr if mesh has no allocation.
    const atlas_region*
    get_region(const utils::id& mesh_id) const;

    // Remap UV2 coordinates from per-mesh [0,1] space to atlas space.
    // uv_in is in [0,1] relative to the mesh's region.
    // Returns UV in [0,1] relative to the full atlas.
    void
    remap_uv2(const utils::id& mesh_id, float& u, float& v) const;

    // Clear all allocations
    void
    clear();

private:
    uint32_t m_width;
    uint32_t m_height;

    // Skyline packing state
    struct skyline_node
    {
        uint32_t x;
        uint32_t y;
        uint32_t width;
    };
    std::vector<skyline_node> m_skyline;

    std::unordered_map<utils::id, atlas_region> m_regions;

    bool
    skyline_pack(uint32_t region_width, uint32_t region_height, uint32_t& out_x, uint32_t& out_y);
};

}  // namespace render
}  // namespace kryga
