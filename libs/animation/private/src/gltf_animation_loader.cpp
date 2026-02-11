#include <animation/gltf_animation_loader.h>

#include <utils/kryga_log.h>

#include <cgltf.h>

#include <glm/gtc/type_ptr.hpp>

#include <cstring>

namespace kryga
{
namespace animation
{

namespace
{

void
read_joint_indices(const cgltf_accessor* accessor, size_t vertex_index, uint32_t out[4])
{
    cgltf_uint raw[4] = {};
    cgltf_accessor_read_uint(accessor, vertex_index, raw, 4);
    for (int i = 0; i < 4; ++i)
    {
        out[i] = raw[i];
    }
}

void
read_bone_weights(const cgltf_accessor* accessor, size_t vertex_index, float out[4])
{
    cgltf_accessor_read_float(accessor, vertex_index, out, 4);
}

}  // namespace

bool
gltf_animation_loader::load(const utils::buffer& buf, gltf_load_result& out)
{
    cgltf_options options = {};
    cgltf_data* data = nullptr;

    auto path_str = buf.get_file().str();

    cgltf_result result = cgltf_parse(&options, buf.data(), (cgltf_size)buf.size(), &data);
    if (result != cgltf_result_success)
    {
        ALOG_ERROR("cgltf: Failed to parse '{}'", path_str);
        return false;
    }

    result = cgltf_load_buffers(&options, data, path_str.c_str());
    if (result != cgltf_result_success)
    {
        ALOG_ERROR("cgltf: Failed to load buffers for '{}'", path_str);
        cgltf_free(data);
        return false;
    }

    result = cgltf_validate(data);
    if (result != cgltf_result_success)
    {
        ALOG_WARN("cgltf: Validation warnings for '{}'", path_str);
    }

    // =========================================================================
    // Extract skin data (inverse bind matrices + joint names)
    // =========================================================================
    if (data->skins_count == 0)
    {
        ALOG_WARN("cgltf: No skins found in '{}'", path_str);
        cgltf_free(data);
        return false;
    }

    const cgltf_skin* skin = &data->skins[0];

    // Extract inverse bind matrices
    out.inverse_bind_matrices.resize(skin->joints_count, glm::mat4(1.0f));
    if (skin->inverse_bind_matrices)
    {
        for (cgltf_size i = 0; i < skin->joints_count; ++i)
        {
            float ibm[16];
            cgltf_accessor_read_float(skin->inverse_bind_matrices, i, ibm, 16);
            out.inverse_bind_matrices[i] = glm::make_mat4(ibm);
        }
    }

    // Extract joint names
    out.joint_names.resize(skin->joints_count);
    for (cgltf_size i = 0; i < skin->joints_count; ++i)
    {
        if (skin->joints[i]->name)
        {
            out.joint_names[i] = skin->joints[i]->name;
        }
    }

    // =========================================================================
    // Extract skinned meshes
    // =========================================================================
    for (cgltf_size ni = 0; ni < data->nodes_count; ++ni)
    {
        cgltf_node* node = &data->nodes[ni];

        if (!node->mesh || !node->skin)
        {
            continue;
        }

        if (node->skin != skin)
        {
            continue;
        }

        cgltf_mesh* mesh = node->mesh;

        for (cgltf_size pi = 0; pi < mesh->primitives_count; ++pi)
        {
            cgltf_primitive* prim = &mesh->primitives[pi];

            if (prim->type != cgltf_primitive_type_triangles)
            {
                continue;
            }

            gltf_mesh_result mesh_result;

            const cgltf_accessor* pos_acc = nullptr;
            const cgltf_accessor* norm_acc = nullptr;
            const cgltf_accessor* uv_acc = nullptr;
            const cgltf_accessor* joints_acc = nullptr;
            const cgltf_accessor* weights_acc = nullptr;

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
                        uv_acc = attr.data;
                    break;
                case cgltf_attribute_type_joints:
                    if (attr.index == 0)
                        joints_acc = attr.data;
                    break;
                case cgltf_attribute_type_weights:
                    if (attr.index == 0)
                        weights_acc = attr.data;
                    break;
                default:
                    break;
                }
            }

            if (!pos_acc)
            {
                ALOG_WARN("cgltf: Mesh primitive missing POSITION attribute");
                continue;
            }

            size_t vertex_count = pos_acc->count;
            mesh_result.vertices.resize(vertex_count);

            for (size_t vi = 0; vi < vertex_count; ++vi)
            {
                auto& v = mesh_result.vertices[vi];

                float pos[3] = {};
                cgltf_accessor_read_float(pos_acc, vi, pos, 3);
                v.position = glm::vec3(pos[0], pos[1], pos[2]);

                if (norm_acc)
                {
                    float norm[3] = {};
                    cgltf_accessor_read_float(norm_acc, vi, norm, 3);
                    v.normal = glm::vec3(norm[0], norm[1], norm[2]);
                }
                else
                {
                    v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                }

                v.color = glm::vec3(1.0f, 1.0f, 1.0f);

                if (uv_acc)
                {
                    float uv[2] = {};
                    cgltf_accessor_read_float(uv_acc, vi, uv, 2);
                    v.uv = glm::vec2(uv[0], uv[1]);
                }

                if (joints_acc)
                {
                    uint32_t ji[4] = {};
                    read_joint_indices(joints_acc, vi, ji);
                    v.bone_indices = glm::uvec4(ji[0], ji[1], ji[2], ji[3]);
                }

                if (weights_acc)
                {
                    float bw[4] = {};
                    read_bone_weights(weights_acc, vi, bw);
                    v.bone_weights = glm::vec4(bw[0], bw[1], bw[2], bw[3]);
                }
            }

            if (prim->indices)
            {
                mesh_result.indices.resize(prim->indices->count);
                for (cgltf_size ii = 0; ii < prim->indices->count; ++ii)
                {
                    mesh_result.indices[ii] =
                        (uint32_t)cgltf_accessor_read_index(prim->indices, ii);
                }
            }

            out.meshes.push_back(std::move(mesh_result));
        }
    }

    ALOG_INFO("cgltf: Loaded '{}' - {} joints, {} meshes", path_str,
              out.joint_names.size(), out.meshes.size());

    cgltf_free(data);
    return true;
}

}  // namespace animation
}  // namespace kryga
