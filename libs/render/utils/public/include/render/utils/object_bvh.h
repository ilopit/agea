#pragma once

#include <glm_unofficial/glm.h>

#include <cstdint>
#include <vector>

namespace kryga
{
namespace render
{

struct ray
{
    glm::vec3 origin;
    glm::vec3 direction;  // normalized
};

struct bvh_object_entry
{
    glm::vec3 aabb_min;
    glm::vec3 aabb_max;
    uint32_t user_id;
    void* user_data;
};

struct raycast_hit
{
    float distance;
    uint32_t user_id;
    void* user_data;
};

// Flat BVH node for object-level spatial queries
struct object_bvh_node
{
    glm::vec3 aabb_min;
    glm::vec3 aabb_max;
    uint32_t left_or_first;  // internal: left child index, leaf: first object index
    uint32_t count;          // 0 = internal node, >0 = leaf with count objects
};

class object_bvh
{
public:
    void
    build(const bvh_object_entry* entries, uint32_t count);

    void
    clear();

    bool
    empty() const
    {
        return m_nodes.empty();
    }

    // Cast ray, return nearest hit. Returns false on miss.
    bool
    raycast(const ray& r, raycast_hit& out_hit) const;

    // Utility: construct a world-space ray from screen coordinates + camera matrices
    static ray
    screen_to_ray(uint32_t screen_x,
                  uint32_t screen_y,
                  uint32_t screen_w,
                  uint32_t screen_h,
                  const glm::mat4& inv_projection,
                  const glm::mat4& inv_view);

private:
    void
    build_recursive(uint32_t node_idx, uint32_t begin, uint32_t end);

    std::vector<object_bvh_node> m_nodes;
    std::vector<bvh_object_entry> m_objects;
};

}  // namespace render
}  // namespace kryga
