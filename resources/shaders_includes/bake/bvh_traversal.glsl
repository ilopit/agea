// BVH traversal for GPU ray tracing in lightmap baker
// Requires bvh_node and bake_triangle SSBO bindings before including

#include "gpu_types/gpu_bvh_types.h"

// Ray-AABB intersection (slab method)
bool ray_aabb(vec3 origin, vec3 inv_dir, vec3 aabb_min, vec3 aabb_max, float t_max)
{
    vec3 t0 = (aabb_min - origin) * inv_dir;
    vec3 t1 = (aabb_max - origin) * inv_dir;

    vec3 tmin = min(t0, t1);
    vec3 tmax = max(t0, t1);

    float enter = max(max(tmin.x, tmin.y), tmin.z);
    float exit_ = min(min(tmax.x, tmax.y), tmax.z);

    return enter <= exit_ && exit_ >= 0.0 && enter < t_max;
}

// Ray-triangle intersection (Moller-Trumbore)
// Returns t, u, v (barycentric). t < 0 means no hit.
vec3 ray_triangle(vec3 origin, vec3 dir, vec3 v0, vec3 v1, vec3 v2)
{
    vec3 e1 = v1 - v0;
    vec3 e2 = v2 - v0;
    vec3 h = cross(dir, e2);
    float a = dot(e1, h);

    if (abs(a) < 1e-7)
        return vec3(-1.0, 0.0, 0.0);

    float f = 1.0 / a;
    vec3 s = origin - v0;
    float u = f * dot(s, h);

    if (u < 0.0 || u > 1.0)
        return vec3(-1.0, 0.0, 0.0);

    vec3 q = cross(s, e1);
    float v = f * dot(dir, q);

    if (v < 0.0 || u + v > 1.0)
        return vec3(-1.0, 0.0, 0.0);

    float t = f * dot(e2, q);

    if (t < 1e-5)
        return vec3(-1.0, 0.0, 0.0);

    return vec3(t, u, v);
}

// Trace a ray through the BVH. Returns hit distance and triangle index.
// tri_idx is set to 0xFFFFFFFF on miss.
struct trace_result
{
    float t;
    uint tri_idx;
    float u, v;
};

#define BVH_STACK_SIZE 32

trace_result trace_ray(vec3 origin, vec3 direction, float t_max)
{
    trace_result res;
    res.t = t_max;
    res.tri_idx = 0xFFFFFFFFu;
    res.u = 0.0;
    res.v = 0.0;

    vec3 inv_dir = 1.0 / direction;

    uint stack[BVH_STACK_SIZE];
    int stack_ptr = 0;
    stack[0] = 0u;  // root node
    stack_ptr = 1;

    while (stack_ptr > 0)
    {
        stack_ptr--;
        uint node_idx = stack[stack_ptr];

        bvh_node node = dyn_bvh_nodes.nodes[node_idx];

        if (!ray_aabb(origin, inv_dir, node.aabb_min, node.aabb_max, res.t))
            continue;

        if (KGPU_BVH_IS_LEAF(node))
        {
            uint tri_start = node.left_or_tri_idx;
            uint tri_count = KGPU_BVH_TRI_COUNT(node);

            for (uint i = 0u; i < tri_count; i++)
            {
                bake_triangle tri = dyn_triangles.tris[tri_start + i];
                vec3 hit = ray_triangle(origin, direction, tri.v0, tri.v1, tri.v2);

                if (hit.x > 0.0 && hit.x < res.t)
                {
                    res.t = hit.x;
                    res.tri_idx = tri_start + i;
                    res.u = hit.y;
                    res.v = hit.z;
                }
            }
        }
        else
        {
            if (stack_ptr + 2 <= BVH_STACK_SIZE)
            {
                stack[stack_ptr++] = node.right_or_count;
                stack[stack_ptr++] = node.left_or_tri_idx;
            }
        }
    }

    return res;
}

// Shadow ray: returns true if path is clear (no hit before t_max)
bool trace_shadow(vec3 origin, vec3 direction, float t_max)
{
    vec3 inv_dir = 1.0 / direction;

    uint stack[BVH_STACK_SIZE];
    int stack_ptr = 1;
    stack[0] = 0u;

    while (stack_ptr > 0)
    {
        stack_ptr--;
        uint node_idx = stack[stack_ptr];

        bvh_node node = dyn_bvh_nodes.nodes[node_idx];

        if (!ray_aabb(origin, inv_dir, node.aabb_min, node.aabb_max, t_max))
            continue;

        if (KGPU_BVH_IS_LEAF(node))
        {
            uint tri_start = node.left_or_tri_idx;
            uint tri_count = KGPU_BVH_TRI_COUNT(node);

            for (uint i = 0u; i < tri_count; i++)
            {
                bake_triangle tri = dyn_triangles.tris[tri_start + i];
                vec3 hit = ray_triangle(origin, direction, tri.v0, tri.v1, tri.v2);

                if (hit.x > 0.0 && hit.x < t_max)
                    return false;  // occluded
            }
        }
        else
        {
            if (stack_ptr + 2 <= BVH_STACK_SIZE)
            {
                stack[stack_ptr++] = node.right_or_count;
                stack[stack_ptr++] = node.left_or_tri_idx;
            }
        }
    }

    return true;  // clear
}
