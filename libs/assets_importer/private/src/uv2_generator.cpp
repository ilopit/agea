#include "assets_importer/uv2_generator.h"

#include <utils/kryga_log.h>

#include <xatlas/xatlas.h>

namespace kryga
{
namespace asset_importer
{
namespace uv2_generator
{

uv2_result
generate_uv2(const gpu::vertex_data* vertices,
             uint32_t vertex_count,
             const gpu::uint* indices,
             uint32_t index_count,
             const uv2_generate_params& params)
{
    uv2_result result;

    if (vertex_count == 0 || index_count == 0)
    {
        ALOG_ERROR("uv2_generator: empty mesh");
        return result;
    }

    xatlas::Atlas* atlas = xatlas::Create();

    // Declare mesh for xatlas
    xatlas::MeshDecl mesh_decl;
    mesh_decl.vertexCount = vertex_count;
    mesh_decl.vertexPositionData = &vertices[0].position;
    mesh_decl.vertexPositionStride = sizeof(gpu::vertex_data);
    mesh_decl.vertexNormalData = &vertices[0].normal;
    mesh_decl.vertexNormalStride = sizeof(gpu::vertex_data);
    mesh_decl.vertexUvData = &vertices[0].uv;
    mesh_decl.vertexUvStride = sizeof(gpu::vertex_data);
    mesh_decl.indexCount = index_count;
    mesh_decl.indexData = indices;
    mesh_decl.indexFormat = xatlas::IndexFormat::UInt32;

    xatlas::AddMeshError error = xatlas::AddMesh(atlas, mesh_decl);
    if (error != xatlas::AddMeshError::Success)
    {
        ALOG_ERROR("uv2_generator: xatlas::AddMesh failed: {}", xatlas::StringForEnum(error));
        xatlas::Destroy(atlas);
        return result;
    }

    // Configure chart generation
    xatlas::ChartOptions chart_options;
    if (params.max_chart_size > 0)
    {
        chart_options.maxChartArea =
            static_cast<float>(params.max_chart_size * params.max_chart_size);
    }

    // Configure atlas packing
    xatlas::PackOptions pack_options;
    pack_options.padding = params.padding;
    pack_options.resolution = params.resolution;
    pack_options.texelsPerUnit = params.texels_per_unit;
    pack_options.bilinear = true;
    pack_options.blockAlign = true;

    xatlas::Generate(atlas, chart_options, pack_options);

    if (atlas->meshCount == 0)
    {
        ALOG_ERROR("uv2_generator: xatlas generated 0 meshes");
        xatlas::Destroy(atlas);
        return result;
    }

    const xatlas::Mesh& output_mesh = atlas->meshes[0];

    result.atlas_width = atlas->width;
    result.atlas_height = atlas->height;

    // Build output vertices with UV2 populated
    result.vertices.resize(output_mesh.vertexCount);
    for (uint32_t i = 0; i < output_mesh.vertexCount; ++i)
    {
        const xatlas::Vertex& xvert = output_mesh.vertexArray[i];
        // xref points back to original vertex
        result.vertices[i] = vertices[xvert.xref];
        // Set UV2 to the generated lightmap UVs (normalized to [0,1])
        if (atlas->width > 0 && atlas->height > 0)
        {
            result.vertices[i].uv2.x = xvert.uv[0] / static_cast<float>(atlas->width);
            result.vertices[i].uv2.y = xvert.uv[1] / static_cast<float>(atlas->height);
        }
    }

    // Build output indices
    result.indices.resize(output_mesh.indexCount);
    for (uint32_t i = 0; i < output_mesh.indexCount; ++i)
    {
        result.indices[i] = output_mesh.indexArray[i];
    }

    result.success = true;

    ALOG_INFO("uv2_generator: generated {}x{} atlas, {} verts (was {}), {} charts",
              atlas->width,
              atlas->height,
              output_mesh.vertexCount,
              vertex_count,
              output_mesh.chartCount);

    xatlas::Destroy(atlas);
    return result;
}

}  // namespace uv2_generator
}  // namespace asset_importer
}  // namespace kryga
