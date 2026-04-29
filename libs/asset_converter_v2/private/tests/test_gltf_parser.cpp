#include <gtest/gtest.h>

#include <asset_converter/converter_context.h>
#include <asset_converter/gltf_parser.h>

#include <gpu_types/gpu_vertex_types.h>

#include <filesystem>
#include <fstream>

using namespace kryga::converter;

namespace
{

size_t
vertex_count(const std::vector<uint8_t>& bytes)
{
    return bytes.size() / sizeof(kryga::gpu::vertex_data);
}

const kryga::gpu::vertex_data*
vertex_data_ptr(const std::vector<uint8_t>& bytes)
{
    return reinterpret_cast<const kryga::gpu::vertex_data*>(bytes.data());
}

}  // namespace

class GltfParserTest : public ::testing::Test
{
protected:
    std::string m_fox_path;

    void
    SetUp() override
    {
        m_fox_path = "../packages/base.apkg/class/meshes/Fox.glb";
        if (!std::filesystem::exists(m_fox_path))
        {
            GTEST_SKIP() << "Fox.glb not found at " << m_fox_path;
        }
    }
};

TEST_F(GltfParserTest, ParseFoxGlb)
{
    parsed_scene scene;
    ASSERT_TRUE(parse_gltf(m_fox_path, scene));

    EXPECT_FALSE(scene.name.empty());
    EXPECT_EQ(scene.name, "Fox");
}

TEST_F(GltfParserTest, ExtractsMeshes)
{
    parsed_scene scene;
    ASSERT_TRUE(parse_gltf(m_fox_path, scene));

    EXPECT_GT(scene.meshes.size(), 0u);

    for (const auto& mesh : scene.meshes)
    {
        EXPECT_FALSE(mesh.name.empty());
        EXPECT_GT(vertex_count(mesh.vertices), 0u);
        EXPECT_GT(mesh.indices.size(), 0u);
        // Indices should be a multiple of 3 (triangles)
        EXPECT_EQ(mesh.indices.size() % 3, 0u);
    }
}

TEST_F(GltfParserTest, ExtractsTextures)
{
    parsed_scene scene;
    ASSERT_TRUE(parse_gltf(m_fox_path, scene));

    // Fox.glb should have at least one texture
    EXPECT_GT(scene.textures.size(), 0u);

    for (const auto& tex : scene.textures)
    {
        EXPECT_FALSE(tex.name.empty());
        EXPECT_GT(tex.width, 0u);
        EXPECT_GT(tex.height, 0u);

        if (tex.embedded)
        {
            EXPECT_EQ(tex.pixels.size(), tex.width * tex.height * 4);
        }
    }
}

TEST_F(GltfParserTest, ExtractsMaterials)
{
    parsed_scene scene;
    ASSERT_TRUE(parse_gltf(m_fox_path, scene));

    EXPECT_GT(scene.materials.size(), 0u);

    for (const auto& mat : scene.materials)
    {
        EXPECT_FALSE(mat.name.empty());
        EXPECT_FALSE(mat.shader_effect.empty());
        EXPECT_GT(mat.shininess, 0.0f);
    }
}

TEST_F(GltfParserTest, ExtractsSceneGraph)
{
    parsed_scene scene;
    ASSERT_TRUE(parse_gltf(m_fox_path, scene));

    EXPECT_GT(scene.nodes.size(), 0u);
    EXPECT_GT(scene.root_nodes.size(), 0u);

    // All root_node indices should be valid
    for (int idx : scene.root_nodes)
    {
        EXPECT_GE(idx, 0);
        EXPECT_LT(static_cast<size_t>(idx), scene.nodes.size());
    }

    // All child indices should be valid
    for (const auto& node : scene.nodes)
    {
        for (int child_idx : node.children)
        {
            EXPECT_GE(child_idx, 0);
            EXPECT_LT(static_cast<size_t>(child_idx), scene.nodes.size());
        }
    }
}

TEST_F(GltfParserTest, VertexDataHasValidPositions)
{
    parsed_scene scene;
    ASSERT_TRUE(parse_gltf(m_fox_path, scene));
    ASSERT_GT(scene.meshes.size(), 0u);

    const auto& mesh = scene.meshes[0];
    size_t vert_count = vertex_count(mesh.vertices);
    const auto* verts = vertex_data_ptr(mesh.vertices);

    bool has_nonzero = false;
    for (size_t i = 0; i < vert_count; ++i)
    {
        const auto& v = verts[i];
        // Positions should be finite
        EXPECT_FALSE(std::isnan(v.position.x));
        EXPECT_FALSE(std::isnan(v.position.y));
        EXPECT_FALSE(std::isnan(v.position.z));
        EXPECT_FALSE(std::isinf(v.position.x));

        if (v.position.x != 0.f || v.position.y != 0.f || v.position.z != 0.f)
        {
            has_nonzero = true;
        }
    }
    EXPECT_TRUE(has_nonzero) << "All vertex positions are zero";
}

TEST(GltfParserErrorTest, NonexistentFileReturnsFalse)
{
    parsed_scene scene;
    EXPECT_FALSE(parse_gltf("nonexistent_file.glb", scene));
}

TEST(GltfParserErrorTest, InvalidFileReturnsFalse)
{
    // Create a temporary invalid file
    auto tmp = std::filesystem::temp_directory_path() / "invalid_test.glb";
    {
        std::ofstream f(tmp, std::ios::binary);
        f << "not a valid glb file";
    }

    parsed_scene scene;
    EXPECT_FALSE(parse_gltf(tmp.string(), scene));

    std::filesystem::remove(tmp);
}
