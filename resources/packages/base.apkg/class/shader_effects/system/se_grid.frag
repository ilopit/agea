#version 450
#extension GL_GOOGLE_include_directive: enable

#include "descriptor_bindings_grid.glsl"

#include "gpu_types/gpu_generic_constants.h"

layout (location = 0) in vec3 in_near_point;
layout (location = 1) in vec3 in_far_point;

layout (location = 0) out vec4 out_color;

float compute_depth(vec3 world_pos)
{
    vec4 clip = dyn_camera_data.obj.projection * dyn_camera_data.obj.view * vec4(world_pos, 1.0);
    return clip.z / clip.w;
}

vec4 grid(vec3 world_pos, float scale)
{
    vec2 coord = world_pos.xz / scale;
    vec2 derivative = fwidth(coord);
    vec2 grid_lines = abs(fract(coord - 0.5) - 0.5) / derivative;
    float line = min(grid_lines.x, grid_lines.y);
    float alpha = 1.0 - min(line, 1.0);

    vec3 color = vec3(0.35);

    // X-axis highlighting (red) — when Z is near 0
    float z_dist = abs(world_pos.z / scale);
    float z_deriv = fwidth(world_pos.z / scale);
    float z_line = z_dist / z_deriv;
    if (z_line < 1.0)
    {
        color = vec3(0.8, 0.15, 0.15);
        alpha = max(alpha, 1.0 - z_line);
    }

    // Z-axis highlighting (blue) — when X is near 0
    float x_dist = abs(world_pos.x / scale);
    float x_deriv = fwidth(world_pos.x / scale);
    float x_line = x_dist / x_deriv;
    if (x_line < 1.0)
    {
        color = vec3(0.15, 0.15, 0.8);
        alpha = max(alpha, 1.0 - x_line);
    }

    return vec4(color, alpha);
}

void main()
{
    // Ray-plane intersection with Y=0 plane
    float t = -in_near_point.y / (in_far_point.y - in_near_point.y);

    if (t < 0.0)
        discard;

    vec3 world_pos = in_near_point + t * (in_far_point - in_near_point);

    // Two-scale grid: thin lines every 1 unit, thicker lines every 10 units
    vec4 grid_small = grid(world_pos, 1.0);
    vec4 grid_large = grid(world_pos, 10.0);

    // Combine: large grid on top of small grid
    vec4 color = grid_small;
    color.a *= 0.4;
    color = vec4(mix(color.rgb, grid_large.rgb, grid_large.a),
                 max(color.a, grid_large.a * 0.7));

    // Distance-based fade
    float dist = length(world_pos.xz - dyn_camera_data.obj.position.xz);
    float fade = 1.0 - smoothstep(50.0, 200.0, dist);
    color.a *= fade;

    if (color.a < 0.01)
        discard;

    // Write depth from world-space intersection point
    float depth = compute_depth(world_pos);
    gl_FragDepth = clamp(depth, 0.0, 1.0);

    out_color = color;
}
