#include <asset_converter/gltf_parser.h>

#include <gpu_types/gpu_vertex_types.h>

#include <cgltf.h>
#include <stb_image.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <unordered_map>
#include <unordered_set>

namespace kryga::converter
{

namespace
{

std::string
sanitize_name(const char* raw, const char* fallback_prefix, int index)
{
    if (raw && raw[0] != '\0')
    {
        std::string s(raw);
        for (auto& c : s)
        {
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_')
            {
                c = '_';
            }
        }
        // ensure doesn't start with digit
        if (std::isdigit(static_cast<unsigned char>(s[0])))
        {
            s = std::string("_") + s;
        }
        return s;
    }
    return std::string(fallback_prefix) + std::to_string(index);
}

// Helper to ensure unique names within a category
class unique_name_tracker
{
public:
    std::string
    make_unique(const std::string& base)
    {
        std::string result = base;
        int suffix = 1;
        while (m_used.count(result) > 0)
        {
            result = base + "_" + std::to_string(suffix);
            ++suffix;
        }
        m_used.insert(result);
        return result;
    }

private:
    std::unordered_set<std::string> m_used;
};

glm::vec3
quat_to_euler_degrees(const glm::quat& q)
{
    glm::vec3 euler = glm::eulerAngles(q);
    return glm::degrees(euler);
}

void
compute_flat_normals(std::vector<gpu::vertex_data>& vertices, const std::vector<uint32_t>& indices)
{
    for (size_t i = 0; i + 2 < indices.size(); i += 3)
    {
        auto& v0 = vertices[indices[i]];
        auto& v1 = vertices[indices[i + 1]];
        auto& v2 = vertices[indices[i + 2]];

        glm::vec3 edge1 = v1.position - v0.position;
        glm::vec3 edge2 = v2.position - v0.position;
        glm::vec3 n = glm::normalize(glm::cross(edge1, edge2));

        v0.normal = n;
        v1.normal = n;
        v2.normal = n;
    }
}

// ============================================================================
// Texture extraction
// ============================================================================

bool
extract_textures(cgltf_data* data,
                 const std::string& base_dir,
                 parsed_scene& scene,
                 unique_name_tracker& tex_names)
{
    for (cgltf_size i = 0; i < data->images_count; ++i)
    {
        cgltf_image* image = &data->images[i];
        parsed_texture tex;
        std::string base_name = sanitize_name(image->name, "txt_", static_cast<int>(i));
        tex.name = tex_names.make_unique(base_name);

        if (image->buffer_view)
        {
            // Embedded image — decode from memory
            const uint8_t* buf_ptr = static_cast<const uint8_t*>(image->buffer_view->buffer->data) +
                                     image->buffer_view->offset;
            size_t buf_size = image->buffer_view->size;

            int w = 0, h = 0, channels = 0;
            uint8_t* pixels =
                stbi_load_from_memory(buf_ptr, static_cast<int>(buf_size), &w, &h, &channels, 4);

            if (!pixels)
            {
                std::cerr << "ERROR: Failed to decode embedded image '" << tex.name << "'\n";
                continue;
            }

            tex.width = static_cast<uint32_t>(w);
            tex.height = static_cast<uint32_t>(h);
            tex.pixels.assign(pixels, pixels + w * h * 4);
            tex.embedded = true;

            stbi_image_free(pixels);
        }
        else if (image->uri)
        {
            // External file reference
            std::string uri(image->uri);

            // Skip data URIs — treat as embedded
            if (uri.rfind("data:", 0) == 0)
            {
                // base64 data URI — decode
                // cgltf should have already decoded this into buffer_view
                std::cerr << "WARN: data URI image not supported via uri path, skipping '"
                          << tex.name << "'\n";
                continue;
            }

            std::filesystem::path img_path =
                std::filesystem::path(base_dir) / std::filesystem::path(uri);

            if (!std::filesystem::exists(img_path))
            {
                std::cerr << "WARN: Image file not found: " << img_path << "\n";
                continue;
            }

            // Get dimensions
            int w = 0, h = 0, channels = 0;
            if (!stbi_info(img_path.string().c_str(), &w, &h, &channels))
            {
                std::cerr << "WARN: Cannot read image info: " << img_path << "\n";
                continue;
            }

            tex.source_path = img_path.string();
            tex.width = static_cast<uint32_t>(w);
            tex.height = static_cast<uint32_t>(h);
            tex.embedded = false;
        }
        else
        {
            std::cerr << "WARN: Image '" << tex.name << "' has no data source, skipping\n";
            continue;
        }

        scene.textures.push_back(std::move(tex));
    }

    return true;
}

// ============================================================================
// Material extraction
// ============================================================================

int
find_texture_index(cgltf_data* data, const parsed_scene& scene, cgltf_texture* tex)
{
    if (!tex || !tex->image)
    {
        return -1;
    }

    cgltf_size image_idx = static_cast<cgltf_size>(tex->image - data->images);
    // Match by name since scene.textures may have skipped some
    std::string expected_name =
        sanitize_name(data->images[image_idx].name, "txt_", static_cast<int>(image_idx));

    for (size_t i = 0; i < scene.textures.size(); ++i)
    {
        if (scene.textures[i].name == expected_name)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool
extract_materials(cgltf_data* data, parsed_scene& scene, unique_name_tracker& mat_names)
{
    for (cgltf_size i = 0; i < data->materials_count; ++i)
    {
        cgltf_material* mat = &data->materials[i];
        parsed_material ir_mat;
        std::string base_name = sanitize_name(mat->name, "mt_", static_cast<int>(i));
        ir_mat.name = mat_names.make_unique(base_name);

        if (mat->has_pbr_metallic_roughness)
        {
            auto& pbr = mat->pbr_metallic_roughness;

            // Base color → diffuse
            ir_mat.diffuse = glm::vec3(
                pbr.base_color_factor[0], pbr.base_color_factor[1], pbr.base_color_factor[2]);

            // Ambient approximation
            ir_mat.ambient = ir_mat.diffuse * 0.2f;

            // Metallic → specular intensity
            float metallic = pbr.metallic_factor;
            ir_mat.specular = glm::vec3(metallic * 0.8f);

            // Roughness → shininess
            float roughness = pbr.roughness_factor;
            float smooth = 1.0f - roughness;
            ir_mat.shininess = smooth * smooth * 128.0f;
            ir_mat.shininess = std::max(ir_mat.shininess, 1.0f);

            // Diffuse texture
            if (pbr.base_color_texture.texture)
            {
                int tex_idx = find_texture_index(data, scene, pbr.base_color_texture.texture);
                if (tex_idx >= 0)
                {
                    ir_mat.diffuse_texture = scene.textures[tex_idx].name;
                }
            }

            // Metallic-roughness texture → specular texture (approximate)
            if (pbr.metallic_roughness_texture.texture)
            {
                int tex_idx =
                    find_texture_index(data, scene, pbr.metallic_roughness_texture.texture);
                if (tex_idx >= 0)
                {
                    ir_mat.specular_texture = scene.textures[tex_idx].name;
                }
            }
        }

        // Choose shader effect based on whether textures are present
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

    return true;
}

// ============================================================================
// Light extraction (KHR_lights_punctual)
// ============================================================================

bool
extract_lights(cgltf_data* data, parsed_scene& scene, unique_name_tracker& light_names)
{
    for (cgltf_size i = 0; i < data->lights_count; ++i)
    {
        cgltf_light* light = &data->lights[i];
        parsed_light ir_l;
        std::string base_name = sanitize_name(light->name, "light_", static_cast<int>(i));
        ir_l.name = light_names.make_unique(base_name);
        ir_l.color = glm::vec3(light->color[0], light->color[1], light->color[2]);
        ir_l.intensity = light->intensity;
        ir_l.range = light->range > 0.f ? light->range : 50.f;

        switch (light->type)
        {
        case cgltf_light_type_directional:
            ir_l.type = parsed_light_type::directional;
            break;
        case cgltf_light_type_point:
            ir_l.type = parsed_light_type::point;
            break;
        case cgltf_light_type_spot:
            ir_l.type = parsed_light_type::spot;
            ir_l.inner_cone = light->spot_inner_cone_angle;
            ir_l.outer_cone = light->spot_outer_cone_angle;
            break;
        default:
            ir_l.type = parsed_light_type::point;
            break;
        }

        scene.lights.push_back(std::move(ir_l));
    }

    return true;
}

// ============================================================================
// Camera extraction
// ============================================================================

bool
extract_cameras(cgltf_data* data, parsed_scene& scene, unique_name_tracker& cam_names)
{
    for (cgltf_size i = 0; i < data->cameras_count; ++i)
    {
        cgltf_camera* cam = &data->cameras[i];
        parsed_camera ir_c;
        std::string base_name = sanitize_name(cam->name, "camera_", static_cast<int>(i));
        ir_c.name = cam_names.make_unique(base_name);

        if (cam->type == cgltf_camera_type_perspective)
        {
            auto& persp = cam->data.perspective;
            ir_c.fov = glm::degrees(persp.yfov);
            ir_c.znear = persp.znear;
            ir_c.zfar = persp.has_zfar ? persp.zfar : 256.f;
            ir_c.aspect_ratio = persp.has_aspect_ratio ? persp.aspect_ratio : 16.f / 9.f;
        }

        scene.cameras.push_back(std::move(ir_c));
    }

    return true;
}

// ============================================================================
// Mesh extraction
// ============================================================================

bool
extract_meshes(cgltf_data* data,
               parsed_scene& scene,
               std::unordered_map<size_t, std::vector<size_t>>& mesh_to_ir,
               unique_name_tracker& mesh_names)
{
    for (cgltf_size mi = 0; mi < data->meshes_count; ++mi)
    {
        cgltf_mesh* mesh = &data->meshes[mi];

        for (cgltf_size pi = 0; pi < mesh->primitives_count; ++pi)
        {
            cgltf_primitive* prim = &mesh->primitives[pi];

            if (prim->type != cgltf_primitive_type_triangles)
            {
                std::cerr << "WARN: Skipping non-triangle primitive in mesh '"
                          << (mesh->name ? mesh->name : "unnamed") << "'\n";
                continue;
            }

            parsed_mesh ir_m;

            std::string base_name = sanitize_name(mesh->name, "mesh_", static_cast<int>(mi));
            if (mesh->primitives_count > 1)
            {
                base_name = base_name + "_p" + std::to_string(pi);
            }
            ir_m.name = mesh_names.make_unique(base_name);

            // Find attribute accessors
            const cgltf_accessor* pos_acc = nullptr;
            const cgltf_accessor* norm_acc = nullptr;
            const cgltf_accessor* uv_acc = nullptr;
            const cgltf_accessor* color_acc = nullptr;

            for (cgltf_size ai = 0; ai < prim->attributes_count; ++ai)
            {
                auto& attr = prim->attributes[ai];
                switch (attr.type)
                {
                case cgltf_attribute_type_position:
                    pos_acc = attr.data;
                    break;
                case cgltf_attribute_type_normal:
                    norm_acc = attr.data;
                    break;
                case cgltf_attribute_type_texcoord:
                    if (attr.index == 0)
                    {
                        uv_acc = attr.data;
                    }
                    break;
                case cgltf_attribute_type_color:
                    if (attr.index == 0)
                    {
                        color_acc = attr.data;
                    }
                    break;
                default:
                    break;
                }
            }

            if (!pos_acc)
            {
                std::cerr << "WARN: Mesh primitive missing POSITION, skipping\n";
                continue;
            }

            size_t vertex_count = pos_acc->count;
            std::vector<gpu::vertex_data> temp_vertices(vertex_count);

            bool needs_flat_normals = (norm_acc == nullptr);

            for (size_t vi = 0; vi < vertex_count; ++vi)
            {
                auto& v = temp_vertices[vi];

                float pos[3] = {};
                cgltf_accessor_read_float(pos_acc, vi, pos, 3);
                v.position = glm::vec3(pos[0], pos[1], pos[2]);

                if (norm_acc)
                {
                    float norm[3] = {};
                    cgltf_accessor_read_float(norm_acc, vi, norm, 3);
                    v.normal = glm::vec3(norm[0], norm[1], norm[2]);
                }

                if (uv_acc)
                {
                    float uv[2] = {};
                    cgltf_accessor_read_float(uv_acc, vi, uv, 2);
                    v.uv = glm::vec2(uv[0], 1.0f - uv[1]);  // flip Y
                }
                else
                {
                    v.uv = glm::vec2(0.0f, 0.0f);
                }

                if (color_acc)
                {
                    float col[4] = {1.f, 1.f, 1.f, 1.f};
                    cgltf_size num_components = cgltf_num_components(color_acc->type);
                    if (num_components > 4)
                    {
                        num_components = 4;
                    }
                    cgltf_accessor_read_float(color_acc, vi, col, num_components);
                    v.color = glm::vec3(col[0], col[1], col[2]);
                }
                else
                {
                    v.color = glm::vec3(1.0f, 1.0f, 1.0f);
                }
            }

            // Indices
            if (prim->indices)
            {
                ir_m.indices.resize(prim->indices->count);
                for (cgltf_size ii = 0; ii < prim->indices->count; ++ii)
                {
                    ir_m.indices[ii] =
                        static_cast<uint32_t>(cgltf_accessor_read_index(prim->indices, ii));
                }
            }
            else
            {
                // No index buffer — generate sequential indices
                ir_m.indices.resize(vertex_count);
                for (size_t ii = 0; ii < vertex_count; ++ii)
                {
                    ir_m.indices[ii] = static_cast<uint32_t>(ii);
                }
            }

            if (needs_flat_normals)
            {
                compute_flat_normals(temp_vertices, ir_m.indices);
            }

            // Convert vertices to raw bytes
            size_t bytes_size = temp_vertices.size() * sizeof(gpu::vertex_data);
            ir_m.vertices.resize(bytes_size);
            std::memcpy(ir_m.vertices.data(), temp_vertices.data(), bytes_size);

            size_t ir_idx = scene.meshes.size();
            mesh_to_ir[mi].push_back(ir_idx);
            scene.meshes.push_back(std::move(ir_m));
        }
    }

    return true;
}

// ============================================================================
// Scene graph extraction
// ============================================================================

int
process_node(cgltf_data* data,
             cgltf_node* node,
             parsed_scene& scene,
             const std::unordered_map<size_t, std::vector<size_t>>& mesh_to_ir,
             unique_name_tracker& node_names)
{
    parsed_node parsed_n;
    std::string base_name =
        sanitize_name(node->name, "node_", static_cast<int>(node - data->nodes));
    parsed_n.name = node_names.make_unique(base_name);

    // Decompose transform
    if (node->has_matrix)
    {
        glm::mat4 m = glm::make_mat4(node->matrix);
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::quat orientation;
        glm::decompose(m,
                       parsed_n.transform.scale,
                       orientation,
                       parsed_n.transform.position,
                       skew,
                       perspective);
        parsed_n.transform.rotation = quat_to_euler_degrees(orientation);
    }
    else
    {
        if (node->has_translation)
        {
            parsed_n.transform.position =
                glm::vec3(node->translation[0], node->translation[1], node->translation[2]);
        }

        if (node->has_rotation)
        {
            glm::quat q(node->rotation[3], node->rotation[0], node->rotation[1], node->rotation[2]);
            parsed_n.transform.rotation = quat_to_euler_degrees(q);
        }

        if (node->has_scale)
        {
            parsed_n.transform.scale = glm::vec3(node->scale[0], node->scale[1], node->scale[2]);
        }
    }

    // Light reference
    if (node->light)
    {
        cgltf_size light_idx = static_cast<cgltf_size>(node->light - data->lights);
        if (light_idx < scene.lights.size())
        {
            parsed_n.light_index = static_cast<int>(light_idx);
        }
    }

    // Camera reference
    if (node->camera)
    {
        cgltf_size cam_idx = static_cast<cgltf_size>(node->camera - data->cameras);
        if (cam_idx < scene.cameras.size())
        {
            parsed_n.camera_index = static_cast<int>(cam_idx);
        }
    }

    // Note: skinned mesh detection removed - not supported in current parsed_node

    // Mesh reference
    if (node->mesh)
    {
        cgltf_size mesh_idx = static_cast<cgltf_size>(node->mesh - data->meshes);
        auto it = mesh_to_ir.find(mesh_idx);
        if (it != mesh_to_ir.end() && !it->second.empty())
        {
            parsed_n.mesh_index = static_cast<int>(it->second[0]);
        }

        // Material from first primitive
        if (node->mesh->primitives_count > 0 && node->mesh->primitives[0].material)
        {
            cgltf_size mat_idx =
                static_cast<cgltf_size>(node->mesh->primitives[0].material - data->materials);
            if (mat_idx < scene.materials.size())
            {
                parsed_n.material_index = static_cast<int>(mat_idx);
            }
        }
    }

    int node_idx = static_cast<int>(scene.nodes.size());
    scene.nodes.push_back(std::move(parsed_n));

    // Recurse children
    for (cgltf_size ci = 0; ci < node->children_count; ++ci)
    {
        int child_idx = process_node(data, node->children[ci], scene, mesh_to_ir, node_names);
        scene.nodes[node_idx].children.push_back(child_idx);
    }

    return node_idx;
}

}  // namespace

// ============================================================================
// Public API
// ============================================================================

bool
parse_gltf(const std::string& path, parsed_scene& out_scene)
{
    cgltf_options options = {};
    cgltf_data* data = nullptr;

    cgltf_result result = cgltf_parse_file(&options, path.c_str(), &data);
    if (result != cgltf_result_success)
    {
        std::cerr << "ERROR: Failed to parse glTF file: " << path << "\n";
        return false;
    }

    result = cgltf_load_buffers(&options, data, path.c_str());
    if (result != cgltf_result_success)
    {
        std::cerr << "ERROR: Failed to load glTF buffers: " << path << "\n";
        cgltf_free(data);
        return false;
    }

    result = cgltf_validate(data);
    if (result != cgltf_result_success)
    {
        std::cerr << "WARN: glTF validation issues in: " << path << "\n";
    }

    // Store source path for .glb passthrough
    out_scene.source_path = path;

    // Derive scene name from filename
    std::filesystem::path fs_path(path);
    out_scene.name = fs_path.stem().string();

    // Base directory for resolving relative texture paths
    std::string base_dir = fs_path.parent_path().string();

    // Name trackers to ensure unique IDs
    unique_name_tracker tex_names, mat_names, light_names, cam_names, mesh_names, node_names;

    // Extract in order: textures → materials → meshes → scene graph
    // (materials reference textures, scene graph references meshes+materials)
    if (!extract_textures(data, base_dir, out_scene, tex_names))
    {
        cgltf_free(data);
        return false;
    }

    if (!extract_materials(data, out_scene, mat_names))
    {
        cgltf_free(data);
        return false;
    }

    if (!extract_lights(data, out_scene, light_names))
    {
        cgltf_free(data);
        return false;
    }

    if (!extract_cameras(data, out_scene, cam_names))
    {
        cgltf_free(data);
        return false;
    }

    std::unordered_map<size_t, std::vector<size_t>> mesh_to_ir;
    if (!extract_meshes(data, out_scene, mesh_to_ir, mesh_names))
    {
        cgltf_free(data);
        return false;
    }

    // Process scene graph
    if (data->scenes_count > 0)
    {
        cgltf_scene* scene = &data->scenes[0];
        for (cgltf_size ni = 0; ni < scene->nodes_count; ++ni)
        {
            int root_idx = process_node(data, scene->nodes[ni], out_scene, mesh_to_ir, node_names);
            out_scene.root_nodes.push_back(root_idx);
        }
    }
    else
    {
        // No scene — process all root-level nodes
        for (cgltf_size ni = 0; ni < data->nodes_count; ++ni)
        {
            // Only process nodes without parents
            bool is_root = true;
            for (cgltf_size nj = 0; nj < data->nodes_count; ++nj)
            {
                for (cgltf_size ci = 0; ci < data->nodes[nj].children_count; ++ci)
                {
                    if (data->nodes[nj].children[ci] == &data->nodes[ni])
                    {
                        is_root = false;
                        break;
                    }
                }
                if (!is_root)
                {
                    break;
                }
            }

            if (is_root)
            {
                int root_idx =
                    process_node(data, &data->nodes[ni], out_scene, mesh_to_ir, node_names);
                out_scene.root_nodes.push_back(root_idx);
            }
        }
    }

    std::cout << "Parsed glTF: " << out_scene.meshes.size() << " meshes, "
              << out_scene.textures.size() << " textures, " << out_scene.materials.size()
              << " materials, " << out_scene.lights.size() << " lights, "
              << out_scene.cameras.size() << " cameras, " << out_scene.nodes.size() << " nodes\n";

    cgltf_free(data);
    return true;
}

}  // namespace kryga::converter
