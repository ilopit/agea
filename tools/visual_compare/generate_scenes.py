"""
Generate a simple shadow test scene: ground plane + sphere + cube.
Outputs shadow_test.glb
Requires: pip install pygltflib numpy
"""
import struct
import json
import base64
import math

def generate_sphere(radius, slices, stacks):
    positions = []
    normals = []
    indices = []

    for i in range(stacks + 1):
        phi = math.pi * i / stacks
        for j in range(slices + 1):
            theta = 2.0 * math.pi * j / slices
            x = radius * math.sin(phi) * math.cos(theta)
            y = radius * math.cos(phi)
            z = radius * math.sin(phi) * math.sin(theta)
            positions.extend([x, y, z])
            l = math.sqrt(x*x + y*y + z*z)
            if l > 0:
                normals.extend([x/l, y/l, z/l])
            else:
                normals.extend([0, 1, 0])

    for i in range(stacks):
        for j in range(slices):
            a = i * (slices + 1) + j
            b = a + slices + 1
            indices.extend([a, a + 1, b])
            indices.extend([a + 1, b + 1, b])

    return positions, normals, indices

def generate_box(sx, sy, sz, offset_y=0):
    hx, hy, hz = sx/2, sy/2, sz/2
    y0 = offset_y
    # Each face: 4 vertices in CCW order when viewed from outside
    positions = []
    normals = []
    indices = []

    faces = [
        # (normal, v0, v1, v2, v3) — CCW from outside
        ([0,1,0],  [-hx,hy+y0,-hz], [-hx,hy+y0,hz], [hx,hy+y0,hz], [hx,hy+y0,-hz]),       # +Y
        ([0,-1,0], [-hx,-hy+y0,-hz], [hx,-hy+y0,-hz], [hx,-hy+y0,hz], [-hx,-hy+y0,hz]),    # -Y
        ([0,0,1],  [-hx,-hy+y0,hz], [hx,-hy+y0,hz], [hx,hy+y0,hz], [-hx,hy+y0,hz]),        # +Z
        ([0,0,-1], [hx,-hy+y0,-hz], [-hx,-hy+y0,-hz], [-hx,hy+y0,-hz], [hx,hy+y0,-hz]),    # -Z
        ([1,0,0],  [hx,-hy+y0,hz], [hx,-hy+y0,-hz], [hx,hy+y0,-hz], [hx,hy+y0,hz]),        # +X
        ([-1,0,0], [-hx,-hy+y0,-hz], [-hx,-hy+y0,hz], [-hx,hy+y0,hz], [-hx,hy+y0,-hz]),    # -X
    ]

    for n, v0, v1, v2, v3 in faces:
        b = len(positions) // 3
        positions.extend(v0 + v1 + v2 + v3)
        normals.extend(n * 4)
        indices.extend([b, b+1, b+2, b, b+2, b+3])

    return positions, normals, indices

def generate_plane(size, y=0):
    h = size / 2
    positions = [-h, y, -h,  h, y, -h,  h, y, h,  -h, y, h]
    normals = [0, 1, 0,  0, 1, 0,  0, 1, 0,  0, 1, 0]
    indices = [0, 2, 1, 0, 3, 2]
    return positions, normals, indices

def pack_floats(data):
    return struct.pack(f'<{len(data)}f', *data)

def pack_indices(data):
    return struct.pack(f'<{len(data)}H', *data)

def build_glb():
    # Generate geometry
    plane_pos, plane_nrm, plane_idx = generate_plane(10.0, 0.0)
    sphere_pos, sphere_nrm, sphere_idx = generate_sphere(0.5, 32, 16)
    # Offset sphere up
    for i in range(1, len(sphere_pos), 3):
        sphere_pos[i] += 0.5
    box_pos, box_nrm, box_idx = generate_box(0.8, 0.8, 0.8, 0.4)

    meshes_data = [
        ("ground", plane_pos, plane_nrm, plane_idx, [0.8, 0.8, 0.8, 1.0], 0.0, 0.9),
        ("sphere", sphere_pos, sphere_nrm, sphere_idx, [0.9, 0.2, 0.2, 1.0], 0.0, 0.4),
        ("cube", box_pos, box_nrm, box_idx, [0.2, 0.4, 0.9, 1.0], 0.5, 0.3),
    ]

    # Build binary buffer
    bin_data = bytearray()
    buffer_views = []
    accessors = []
    meshes = []
    materials = []
    nodes = []

    for i, (name, pos, nrm, idx, color, metallic, roughness) in enumerate(meshes_data):
        pos_bytes = pack_floats(pos)
        nrm_bytes = pack_floats(nrm)
        idx_bytes = pack_indices(idx)
        # Pad to 4 bytes
        while len(idx_bytes) % 4 != 0:
            idx_bytes += b'\x00'

        pos_offset = len(bin_data)
        bin_data.extend(pos_bytes)
        nrm_offset = len(bin_data)
        bin_data.extend(nrm_bytes)
        idx_offset = len(bin_data)
        bin_data.extend(idx_bytes)

        vert_count = len(pos) // 3

        # Compute bounds
        min_pos = [min(pos[j::3]) for j in range(3)]
        max_pos = [max(pos[j::3]) for j in range(3)]

        bv_pos = len(buffer_views)
        buffer_views.append({"buffer": 0, "byteOffset": pos_offset, "byteLength": len(pos_bytes), "target": 34962})
        buffer_views.append({"buffer": 0, "byteOffset": nrm_offset, "byteLength": len(nrm_bytes), "target": 34962})
        buffer_views.append({"buffer": 0, "byteOffset": idx_offset, "byteLength": len(idx) * 2, "target": 34963})

        acc_pos = len(accessors)
        accessors.append({"bufferView": bv_pos, "componentType": 5126, "count": vert_count, "type": "VEC3", "min": min_pos, "max": max_pos})
        accessors.append({"bufferView": bv_pos + 1, "componentType": 5126, "count": vert_count, "type": "VEC3"})
        accessors.append({"bufferView": bv_pos + 2, "componentType": 5123, "count": len(idx), "type": "SCALAR"})

        mat_idx = len(materials)
        materials.append({
            "name": f"{name}_mat",
            "pbrMetallicRoughness": {
                "baseColorFactor": color,
                "metallicFactor": metallic,
                "roughnessFactor": roughness
            }
        })

        meshes.append({
            "name": name,
            "primitives": [{
                "attributes": {"POSITION": acc_pos, "NORMAL": acc_pos + 1},
                "indices": acc_pos + 2,
                "material": mat_idx
            }]
        })

        translation = [0, 0, 0]
        if name == "sphere":
            translation = [-1.0, 0, 0]
        elif name == "cube":
            translation = [1.0, 0, 0]

        nodes.append({"name": name, "mesh": i, "translation": translation})

    scene_node = {"name": "scene", "children": list(range(len(nodes)))}
    nodes.append(scene_node)

    gltf = {
        "asset": {"version": "2.0", "generator": "kryga_shadow_test"},
        "scene": 0,
        "scenes": [{"nodes": [len(nodes) - 1]}],
        "nodes": nodes,
        "meshes": meshes,
        "materials": materials,
        "accessors": accessors,
        "bufferViews": buffer_views,
        "buffers": [{"byteLength": len(bin_data)}],
    }

    # Build GLB
    json_str = json.dumps(gltf, separators=(',', ':'))
    json_bytes = json_str.encode('utf-8')
    # Pad JSON to 4 bytes
    while len(json_bytes) % 4 != 0:
        json_bytes += b' '
    # Pad bin to 4 bytes
    while len(bin_data) % 4 != 0:
        bin_data += b'\x00'

    total_length = 12 + 8 + len(json_bytes) + 8 + len(bin_data)

    glb = bytearray()
    # Header
    glb.extend(struct.pack('<III', 0x46546C67, 2, total_length))
    # JSON chunk
    glb.extend(struct.pack('<II', len(json_bytes), 0x4E4F534A))
    glb.extend(json_bytes)
    # BIN chunk
    glb.extend(struct.pack('<II', len(bin_data), 0x004E4942))
    glb.extend(bin_data)

    with open('shadow_test.glb', 'wb') as f:
        f.write(glb)
    print(f"Generated shadow_test.glb ({len(glb)} bytes)")

if __name__ == '__main__':
    build_glb()
