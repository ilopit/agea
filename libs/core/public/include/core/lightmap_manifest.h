#pragma once

#include <utils/id.h>
#include <vfs/rid.h>

#include <glm_unofficial/glm.h>

#include <cstdint>
#include <unordered_map>

namespace kryga
{
namespace core
{

struct lightmap_object_entry
{
    uint32_t region_x = 0;
    uint32_t region_y = 0;
    uint32_t region_w = 0;
    uint32_t region_h = 0;
    glm::vec2 lightmap_scale{0.0f};
    glm::vec2 lightmap_offset{0.0f};
};

struct lightmap_manifest
{
    uint32_t atlas_width = 0;
    uint32_t atlas_height = 0;

    // component_id → lightmap entry
    std::unordered_map<utils::id, lightmap_object_entry> objects;

    bool
    save(const vfs::rid& id) const;

    bool
    load(const vfs::rid& id);

    const lightmap_object_entry*
    find_entry(const utils::id& component_id) const;
};

}  // namespace core
}  // namespace kryga
