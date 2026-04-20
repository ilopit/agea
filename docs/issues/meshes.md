# Mesh System — Known Issues

## 1. All static meshes carry UV2 even when not lightmapped [medium, memory]

`vertex_data` unconditionally includes `vec2 uv2` (8 bytes/vertex). Meshes that never use lightmaps (UI quads, debug lines, grid, billboards, any non-baked geometry) waste 8 bytes per vertex in GPU memory and vertex fetch bandwidth.

Current `vertex_data` is 44 bytes. With `uv2` it's 52 bytes — ~18% overhead for non-lightmapped meshes.

**Alternative — vertex stream splitting**: Split UV2 into a separate VkBuffer (stream/binding 1) instead of interleaving it into the vertex struct. The pipeline layout declares both bindings always, so no pipeline permutation explosion. At draw time, lightmapped meshes bind the real UV2 buffer, others bind a tiny dummy buffer.

Changes needed:
1. Remove `uv2` from `vertex_data` (back to 44 bytes)
2. `gpu_types.cpp` `get_default_vertex_inout_layout()` — add binding 1 for `in_lightmap_uv` (location 4)
3. `uv2_generator` — write UV2 into a separate buffer instead of `vertex_data.uv2`
4. Mesh storage — add optional `vbo_uv2` per mesh in `vulkan_render_loader`
5. Draw binding — bind `vbo_uv2` or a zeroed dummy buffer at binding 1
6. Shaders unchanged — `layout(location=4) in vec2 in_lightmap_uv` still works, just sourced from binding 1

This is what Unreal does with `FStaticMeshVertexBuffers` — separate streams for position, tangent basis, UV channels, color.

## 2. Mesh importer overwrites data when OBJ has multiple shapes [medium, correctness]

`mesh_importer.cpp:53` — `vertices.resize(3 * shapes[s].mesh.num_face_vertices.size())` inside the shapes loop resets the buffer each iteration. Only the last shape's vertices survive. Multi-shape OBJ files silently lose geometry.

**Fix**: Accumulate across shapes instead of resizing per shape — pre-compute total face count, resize once, write at increasing offsets.

## 3. Mesh importer crashes on OBJ files missing normals, texcoords, or colors [medium, robustness]

`mesh_importer.cpp:72-83` — Accesses `attrib.normals`, `attrib.texcoords`, and `attrib.colors` arrays without bounds checking. If an OBJ file omits vertex normals, texcoords, or colors, the indices are invalid (-1 or out of range) and the array access is undefined behavior.

**Fix**: Check `idx.normal_index >= 0`, `idx.texcoord_index >= 0`, and `!attrib.colors.empty()` before access. Default to zero normal / zero UV / white color when missing.
