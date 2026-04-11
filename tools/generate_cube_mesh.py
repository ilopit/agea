#!/usr/bin/env python
"""
Generate cube_mesh with proper UV2 (lightmap UVs) in the engine's native format.
Writes cube_mesh.avrt (vertex data) and cube_mesh.aind (index data).
"""
import struct
import os

# vertex_data layout (scalar, no padding):
#   vec3 position  (12 bytes)
#   vec3 normal    (12 bytes)
#   vec3 color     (12 bytes)
#   vec2 uv        (8 bytes)
#   vec2 uv2       (8 bytes)
# Total: 52 bytes per vertex

def pack_vertex(pos, normal, color, uv, uv2):
    return struct.pack('<3f3f3f2f2f',
        pos[0], pos[1], pos[2],
        normal[0], normal[1], normal[2],
        color[0], color[1], color[2],
        uv[0], uv[1],
        uv2[0], uv2[1])

# 6 faces, each gets a cell in a 3x2 grid for UV2
cw = 1.0 / 3.0  # cell width
ch = 1.0 / 2.0  # cell height

faces = [
    # (normal, vertices as (pos, uv), uv2_col, uv2_row)
    # Front face (col 0, row 0)
    ((0,0,1), [(-0.5,-0.5,0.5), (0.5,-0.5,0.5), (0.5,0.5,0.5), (-0.5,0.5,0.5)],
     [(0,0),(1,0),(1,1),(0,1)], 0, 0),
    # Back face (col 1, row 0)
    ((0,0,-1), [(0.5,-0.5,-0.5), (-0.5,-0.5,-0.5), (-0.5,0.5,-0.5), (0.5,0.5,-0.5)],
     [(0,0),(1,0),(1,1),(0,1)], 1, 0),
    # Top face (col 2, row 0)
    ((0,1,0), [(-0.5,0.5,0.5), (0.5,0.5,0.5), (0.5,0.5,-0.5), (-0.5,0.5,-0.5)],
     [(0,0),(1,0),(1,1),(0,1)], 2, 0),
    # Bottom face (col 0, row 1)
    ((0,-1,0), [(-0.5,-0.5,-0.5), (0.5,-0.5,-0.5), (0.5,-0.5,0.5), (-0.5,-0.5,0.5)],
     [(0,0),(1,0),(1,1),(0,1)], 0, 1),
    # Right face (col 1, row 1)
    ((1,0,0), [(0.5,-0.5,0.5), (0.5,-0.5,-0.5), (0.5,0.5,-0.5), (0.5,0.5,0.5)],
     [(0,0),(1,0),(1,1),(0,1)], 1, 1),
    # Left face (col 2, row 1)
    ((-1,0,0), [(-0.5,-0.5,-0.5), (-0.5,-0.5,0.5), (-0.5,0.5,0.5), (-0.5,0.5,-0.5)],
     [(0,0),(1,0),(1,1),(0,1)], 2, 1),
]

vertices = bytearray()
indices = bytearray()
vert_idx = 0

# Padding between UV charts to prevent bilinear bleed (fraction of cell)
pad = 0.06

for normal, positions, uvs, col, row in faces:
    for i in range(4):
        pos = positions[i]
        uv = uvs[i]
        # UV2: map into the 3x2 grid cell with padding
        uv2_x = col * cw + (pad + uv[0] * (1.0 - 2.0 * pad)) * cw
        uv2_y = row * ch + (pad + uv[1] * (1.0 - 2.0 * pad)) * ch
        vertices += pack_vertex(pos, normal, (1,1,1), uv, (uv2_x, uv2_y))

    # Two triangles per face: 0,1,2 and 2,3,0
    for idx in [0, 1, 2, 2, 3, 0]:
        indices += struct.pack('<I', vert_idx + idx)
    vert_idx += 4

# Write files
out_dir = os.path.join(os.path.dirname(__file__), '..', 'resources', 'packages', 'base.apkg', 'class', 'meshes')

avrt_path = os.path.join(out_dir, 'cube_mesh.avrt')
aind_path = os.path.join(out_dir, 'cube_mesh.aind')

with open(avrt_path, 'wb') as f:
    f.write(vertices)

with open(aind_path, 'wb') as f:
    f.write(indices)

print(f"Written {len(vertices)} bytes ({len(vertices)//52} vertices) to {avrt_path}")
print(f"Written {len(indices)} bytes ({len(indices)//4} indices) to {aind_path}")
print(f"Vertex size: 52 bytes, Total: {len(vertices)//52} verts, {len(indices)//4} indices")
