#include <asset_converter/obj_parser.h>

#include <gpu_types/gpu_vertex_types.h>

#include <stb_image.h>
#include <tiny_obj_loader.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <unordered_map>

namespace kryga::converter
{

namespace
{

struct vertex_key
{
    int vi, ni, ti;

    bool
    operator==(const vertex_key& o) const
    {
        return vi == o.vi && ni == o.ni && ti == o.ti;
    }
};

struct vertex_key_hash
{
    size_t
    operator()(const vertex_key& k) const
    {
        size_t h = std::hash<int>{}(k.vi);
        h ^= std::hash<int>{}(k.ni) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(k.ti) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

void
extract_materials_from_mtl(const std::vector<tinyobj::material_t>& materials,
                           const std::string& base_dir,
                           parsed_scene& scene)
{
    for (size_t i = 0; i < materials.size(); ++i)
    {
        const auto& mtl = materials[i];
        parsed_material ir_mat;

        if (!mtl.name.empty())
        {
            ir_mat.name = mtl.name;
        }
        else
        {
            ir_mat.name = "mt_" + std::to_string(i);
        }

        ir_mat.ambient = glm::vec3(mtl.ambient[0], mtl.ambient[1], mtl.ambient[2]);
        ir_mat.diffuse = glm::vec3(mtl.diffuse[0], mtl.diffuse[1], mtl.diffuse[2]);
        ir_mat.specular = glm::vec3(mtl.specular[0], mtl.specular[1], mtl.specular[2]);
        ir_mat.shininess = mtl.shininess;
        if (ir_mat.shininess <= 0.0f)
        {
            ir_mat.shininess = 64.0f;
        }

        // Diffuse texture
        if (!mtl.diffuse_texname.empty())
        {
            std::filesystem::path tex_path = std::filesystem::path(base_dir) / mtl.diffuse_texname;

            if (std::filesystem::exists(tex_path))
            {
                // Create texture entry
                parsed_texture tex;
                tex.name = "txt_" + ir_mat.name;
                tex.source_path = tex_path.string();
                tex.embedded = false;

                int w = 0, h = 0, ch = 0;
                if (stbi_info(tex_path.string().c_str(), &w, &h, &ch))
                {
                    tex.width = static_cast<uint32_t>(w);
                    tex.height = static_cast<uint32_t>(h);
                }

                ir_mat.diffuse_texture = tex.name;
                scene.textures.push_back(std::move(tex));
            }
        }

        // Specular texture
        if (!mtl.specular_texname.empty())
        {
            std::filesystem::path tex_path = std::filesystem::path(base_dir) / mtl.specular_texname;

            if (std::filesystem::exists(tex_path))
            {
                parsed_texture tex;
                tex.name = "txt_" + ir_mat.name + "_spec";
                tex.source_path = tex_path.string();
                tex.embedded = false;

                int w = 0, h = 0, ch = 0;
                if (stbi_info(tex_path.string().c_str(), &w, &h, &ch))
                {
                    tex.width = static_cast<uint32_t>(w);
                    tex.height = static_cast<uint32_t>(h);
                }

                ir_mat.specular_texture = tex.name;
                scene.textures.push_back(std::move(tex));
            }
        }

        // Choose shader
        if (!ir_mat.diffuse_texture.empty())
        {
            ir_mat.shader_effect = "se_simple_texture_lit";
        }
        else
        {
            ir_mat.shader_effect = "se_solid_color_lit";
        }

        scene.materials.push_back(std::move(ir_mat));
    }
}

}  // namespace

bool
parse_obj(const std::string& path, parsed_scene& out_scene)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    std::filesystem::path fs_path(path);
    std::string base_dir = fs_path.parent_path().string();
    out_scene.name = fs_path.stem().string();

    bool ok = tinyobj::LoadObj(&attrib,
                               &shapes,
                               &materials,
                               &warn,
                               &err,
                               path.c_str(),
                               base_dir.empty() ? nullptr : base_dir.c_str());

    if (!warn.empty())
    {
        std::cerr << "WARN: " << warn << "\n";
    }

    if (!err.empty())
    {
        std::cerr << "ERROR: " << err << "\n";
        return false;
    }

    if (!ok)
    {
        return false;
    }

    // Extract materials from MTL
    extract_materials_from_mtl(materials, base_dir, out_scene);

    // Extract meshes — one parsed_mesh per shape, with vertex deduplication
    for (size_t si = 0; si < shapes.size(); ++si)
    {
        const auto& shape = shapes[si];
        parsed_mesh ir_m;
        std::vector<gpu::vertex_data> temp_vertices;

        if (!shape.name.empty())
        {
            ir_m.name = shape.name;
        }
        else
        {
            ir_m.name = "mesh_" + std::to_string(si);
        }

        std::unordered_map<vertex_key, uint32_t, vertex_key_hash> dedup_map;

        size_t index_offset = 0;
        int shape_material = -1;

        for (size_t fi = 0; fi < shape.mesh.num_face_vertices.size(); ++fi)
        {
            uint32_t fv = shape.mesh.num_face_vertices[fi];

            if (fv != 3)
            {
                std::cerr << "WARN: Non-triangle face (fv=" << fv << ") in shape '" << shape.name
                          << "', skipping\n";
                index_offset += fv;
                continue;
            }

            // Track material (use first face's material)
            if (shape_material < 0 && !shape.mesh.material_ids.empty())
            {
                shape_material = shape.mesh.material_ids[fi];
            }

            for (uint32_t vi = 0; vi < fv; ++vi)
            {
                tinyobj::index_t idx = shape.mesh.indices[index_offset + vi];

                vertex_key key{
                    .vi = idx.vertex_index, .ni = idx.normal_index, .ti = idx.texcoord_index};

                auto it = dedup_map.find(key);
                if (it != dedup_map.end())
                {
                    ir_m.indices.push_back(it->second);
                }
                else
                {
                    gpu::vertex_data vert{};

                    if (idx.vertex_index >= 0)
                    {
                        vert.position.x = attrib.vertices[3 * idx.vertex_index + 0];
                        vert.position.y = attrib.vertices[3 * idx.vertex_index + 1];
                        vert.position.z = attrib.vertices[3 * idx.vertex_index + 2];
                    }

                    if (idx.normal_index >= 0 &&
                        static_cast<size_t>(3 * idx.normal_index + 2) < attrib.normals.size())
                    {
                        vert.normal.x = attrib.normals[3 * idx.normal_index + 0];
                        vert.normal.y = attrib.normals[3 * idx.normal_index + 1];
                        vert.normal.z = attrib.normals[3 * idx.normal_index + 2];
                    }

                    if (idx.texcoord_index >= 0 &&
                        static_cast<size_t>(2 * idx.texcoord_index + 1) < attrib.texcoords.size())
                    {
                        vert.uv.x = attrib.texcoords[2 * idx.texcoord_index + 0];
                        vert.uv.y = 1.0f - attrib.texcoords[2 * idx.texcoord_index + 1];
                    }

                    if (idx.vertex_index >= 0 &&
                        static_cast<size_t>(3 * idx.vertex_index + 2) < attrib.colors.size())
                    {
                        vert.color.x = attrib.colors[3 * idx.vertex_index + 0];
                        vert.color.y = attrib.colors[3 * idx.vertex_index + 1];
                        vert.color.z = attrib.colors[3 * idx.vertex_index + 2];
                    }
                    else
                    {
                        vert.color = glm::vec3(1.0f, 1.0f, 1.0f);
                    }

                    auto new_idx = static_cast<uint32_t>(temp_vertices.size());
                    temp_vertices.push_back(vert);
                    ir_m.indices.push_back(new_idx);
                    dedup_map[key] = new_idx;
                }
            }

            index_offset += fv;
        }

        // Compute flat normals if OBJ had none
        if (attrib.normals.empty())
        {
            for (size_t i = 0; i + 2 < ir_m.indices.size(); i += 3)
            {
                auto& v0 = temp_vertices[ir_m.indices[i]];
                auto& v1 = temp_vertices[ir_m.indices[i + 1]];
                auto& v2 = temp_vertices[ir_m.indices[i + 2]];

                glm::vec3 edge1 = v1.position - v0.position;
                glm::vec3 edge2 = v2.position - v0.position;
                glm::vec3 n = glm::normalize(glm::cross(edge1, edge2));

                v0.normal = n;
                v1.normal = n;
                v2.normal = n;
            }
        }

        // Convert vertices to raw bytes
        size_t bytes_size = temp_vertices.size() * sizeof(gpu::vertex_data);
        ir_m.vertices.resize(bytes_size);
        std::memcpy(ir_m.vertices.data(), temp_vertices.data(), bytes_size);

        // Create a node for this shape
        parsed_node node;
        node.name = ir_m.name;
        node.mesh_index = static_cast<int>(out_scene.meshes.size());
        if (shape_material >= 0 && static_cast<size_t>(shape_material) < out_scene.materials.size())
        {
            node.material_index = shape_material;
        }

        int node_idx = static_cast<int>(out_scene.nodes.size());
        out_scene.nodes.push_back(std::move(node));
        out_scene.root_nodes.push_back(node_idx);

        out_scene.meshes.push_back(std::move(ir_m));
    }

    std::cout << "Parsed OBJ: " << out_scene.meshes.size() << " meshes, "
              << out_scene.textures.size() << " textures, " << out_scene.materials.size()
              << " materials\n";

    return true;
}

}  // namespace kryga::converter
