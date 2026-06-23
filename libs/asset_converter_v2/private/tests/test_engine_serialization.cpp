#include <gtest/gtest.h>

#include <asset_converter/converter_context.h>
#include <asset_converter/gltf_parser.h>
#include <project_paths/project_paths.h>

#include <gpu_types/gpu_vertex_types.h>
#include <utils/id.h>
#include <vfs/rid.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using namespace kryga::converter;

namespace
{

std::vector<uint8_t>
make_vertex_bytes(const std::vector<kryga::gpu::vertex_data>& verts)
{
    std::vector<uint8_t> bytes(verts.size() * sizeof(kryga::gpu::vertex_data));
    std::memcpy(bytes.data(), verts.data(), bytes.size());
    return bytes;
}

fs::path
get_test_assets_dir()
{
    auto layout = kryga::paths::resolve();
    if (layout && !layout->source_root.empty())
    {
        auto p = layout->source_root / "build" / "test_assets" / "converter";
        if (fs::exists(p))
        {
            return p;
        }
    }
    return {};
}

fs::path
get_data_root()
{
    auto layout = kryga::paths::resolve();
    if (layout && !layout->source_root.empty())
    {
        auto resources = layout->source_root / "resources";
        if (fs::exists(resources / "packages" / "root.apkg"))
        {
            return resources;
        }
    }
    return {};
}

}  // namespace

class EngineSerializationTest : public ::testing::Test
{
protected:
    fs::path m_output_dir;
    fs::path m_data_root;
    parsed_scene m_scene;

    void
    SetUp() override
    {
        m_output_dir = fs::temp_directory_path() / "kryga_engine_ser_test";
        fs::remove_all(m_output_dir);
        fs::create_directories(m_output_dir);

        m_data_root = get_data_root();

        // Build a minimal test scene
        parsed_mesh mesh;
        mesh.name = "test_cube";
        mesh.vertices = make_vertex_bytes({
            {.position = glm::vec3(0, 0, 0),
             .normal = glm::vec3(0, 1, 0),
             .color = glm::vec3(1, 1, 1),
             .uv = glm::vec2(0, 0)},
            {.position = glm::vec3(1, 0, 0),
             .normal = glm::vec3(0, 1, 0),
             .color = glm::vec3(1, 1, 1),
             .uv = glm::vec2(1, 0)},
            {.position = glm::vec3(0, 1, 0),
             .normal = glm::vec3(0, 1, 0),
             .color = glm::vec3(1, 1, 1),
             .uv = glm::vec2(0, 1)},
        });
        mesh.indices = {0, 1, 2};
        m_scene.meshes.push_back(std::move(mesh));

        parsed_texture tex;
        tex.name = "test_texture";
        tex.width = 2;
        tex.height = 2;
        tex.embedded = true;
        tex.pixels = std::vector<uint8_t>(2 * 2 * 4, 255);  // White 2x2 RGBA texture
        m_scene.textures.push_back(std::move(tex));

        parsed_material mat;
        mat.name = "test_mat";
        mat.shader_effect = "se_solid_color_lit";
        mat.ambient = glm::vec3(0.2f);
        mat.diffuse = glm::vec3(0.8f);
        mat.specular = glm::vec3(0.5f, 0.3f, 0.1f);
        mat.shininess = 32.0f;
        mat.diffuse_texture = "test_texture";
        m_scene.materials.push_back(std::move(mat));

        parsed_node node;
        node.name = "test_node";
        node.mesh_index = 0;
        node.material_index = 0;
        m_scene.nodes.push_back(std::move(node));
        m_scene.root_nodes.push_back(0);

        m_scene.name = "test_scene";
    }

    void
    TearDown() override
    {
        fs::remove_all(m_output_dir);
    }

    std::string
    read_file(const fs::path& path)
    {
        std::ifstream f(path);
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }
};

TEST_F(EngineSerializationTest, ConverterContextInitializes)
{
    if (m_data_root.empty())
    {
        GTEST_SKIP() << "Data root not found";
    }

    converter_context ctx;
    ASSERT_TRUE(ctx.init(m_data_root, m_output_dir));
    ctx.shutdown();
}

TEST_F(EngineSerializationTest, CreatesPackageWithEngineSerialization)
{
    if (m_data_root.empty())
    {
        GTEST_SKIP() << "Data root not found";
    }

    converter_context ctx;
    ASSERT_TRUE(ctx.init(m_data_root, m_output_dir));

    converter_options opts;
    opts.name = AID("test_pkg");
    opts.mode = converter_mode::package_new;
    opts.output_root = kryga::vfs::rid("output", "");

    ASSERT_TRUE(ctx.convert(m_scene, opts));

    ctx.shutdown();

    // Verify package structure
    fs::path pkg_dir = m_output_dir / "test_pkg.apkg";
    EXPECT_TRUE(fs::exists(pkg_dir)) << "Package directory should exist";
    EXPECT_TRUE(fs::exists(pkg_dir / "package.acfg")) << "package.acfg should exist";
    EXPECT_TRUE(fs::exists(pkg_dir / "class" / "meshes")) << "meshes dir should exist";
    EXPECT_TRUE(fs::exists(pkg_dir / "class" / "materials")) << "materials dir should exist";
    EXPECT_TRUE(fs::exists(pkg_dir / "class" / "textures")) << "textures dir should exist";
    EXPECT_TRUE(fs::exists(pkg_dir / "binaries")) << "binaries dir should exist";
}

TEST_F(EngineSerializationTest, WritesBinaryBuffers)
{
    if (m_data_root.empty())
    {
        GTEST_SKIP() << "Data root not found";
    }

    converter_context ctx;
    ASSERT_TRUE(ctx.init(m_data_root, m_output_dir));

    converter_options opts;
    opts.name = AID("test_pkg");
    opts.mode = converter_mode::package_new;
    opts.output_root = kryga::vfs::rid("output", "");

    ASSERT_TRUE(ctx.convert(m_scene, opts));

    ctx.shutdown();

    // Check binaries directory has files
    fs::path bin_dir = m_output_dir / "test_pkg.apkg" / "binaries";
    ASSERT_TRUE(fs::exists(bin_dir));

    int bin_count = 0;
    for (const auto& entry : fs::directory_iterator(bin_dir))
    {
        if (entry.path().extension() == ".abin")
        {
            ++bin_count;
        }
    }

    // Should have at least: texture buffer, mesh vertices, mesh indices
    EXPECT_GE(bin_count, 3) << "Should have at least 3 binary files";
}

TEST_F(EngineSerializationTest, WritesValidObjectFiles)
{
    if (m_data_root.empty())
    {
        GTEST_SKIP() << "Data root not found";
    }

    converter_context ctx;
    ASSERT_TRUE(ctx.init(m_data_root, m_output_dir));

    converter_options opts;
    opts.name = AID("test_pkg");
    opts.mode = converter_mode::package_new;
    opts.output_root = kryga::vfs::rid("output", "");

    ASSERT_TRUE(ctx.convert(m_scene, opts));

    ctx.shutdown();

    fs::path pkg_dir = m_output_dir / "test_pkg.apkg";

    // Find and check mesh file
    fs::path mesh_file = pkg_dir / "class" / "meshes" / "test_cube.aobj";
    ASSERT_TRUE(fs::exists(mesh_file)) << "Mesh file should exist";

    std::string mesh_content = read_file(mesh_file);
    EXPECT_NE(mesh_content.find("proto_id: mesh"), std::string::npos);
    EXPECT_NE(mesh_content.find("id: test_cube"), std::string::npos);
    EXPECT_NE(mesh_content.find("vertices:"), std::string::npos);
    EXPECT_NE(mesh_content.find("indices:"), std::string::npos);
    EXPECT_NE(mesh_content.find(".abin"), std::string::npos);

    // Find and check texture file
    fs::path tex_file = pkg_dir / "class" / "textures" / "txt_test_texture.aobj";
    ASSERT_TRUE(fs::exists(tex_file)) << "Texture file should exist";

    std::string tex_content = read_file(tex_file);
    EXPECT_NE(tex_content.find("proto_id: texture"), std::string::npos);
    EXPECT_NE(tex_content.find("width: 2"), std::string::npos);
    EXPECT_NE(tex_content.find("height: 2"), std::string::npos);
    EXPECT_NE(tex_content.find("base_color:"), std::string::npos);
    EXPECT_NE(tex_content.find(".abin"), std::string::npos);
}

TEST_F(EngineSerializationTest, ConvertGltfWithEngineSerialization)
{
    fs::path test_assets = get_test_assets_dir();
    if (test_assets.empty() || m_data_root.empty())
    {
        GTEST_SKIP() << "Test assets or data root not found";
    }

    fs::path fox_path = test_assets / "Fox.glb";
    if (!fs::exists(fox_path))
    {
        GTEST_SKIP() << "Fox.glb not found";
    }

    // Parse the glTF
    parsed_scene scene;
    ASSERT_TRUE(parse_gltf(fox_path.string(), scene));

    // Convert using engine serialization
    converter_context ctx;
    ASSERT_TRUE(ctx.init(m_data_root, m_output_dir));

    converter_options opts;
    opts.name = AID("fox");
    opts.mode = converter_mode::package_new;
    opts.output_root = kryga::vfs::rid("output", "");

    ASSERT_TRUE(ctx.convert(scene, opts));

    ctx.shutdown();

    // Verify output
    fs::path pkg_dir = m_output_dir / "fox.apkg";
    EXPECT_TRUE(fs::exists(pkg_dir));
    EXPECT_TRUE(fs::exists(pkg_dir / "package.acfg"));
    EXPECT_TRUE(fs::exists(pkg_dir / "binaries"));

    // Count binary files
    int bin_count = 0;
    for (const auto& entry : fs::directory_iterator(pkg_dir / "binaries"))
    {
        if (entry.path().extension() == ".abin")
        {
            ++bin_count;
        }
    }
    EXPECT_GE(bin_count, 1) << "Should have binary files";
}
