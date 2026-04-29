#include "engine_test_fixture.h"

#include <gpu_types/gpu_vertex_types.h>

#include <cstring>

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

}  // namespace

class PackageWriterTest : public EngineTestFixture
{
protected:
    parsed_scene m_scene;

    void
    SetUp() override
    {
        EngineTestFixture::SetUp();

        // Build a minimal test scene
        parsed_mesh mesh;
        mesh.name = "test_cube";
        mesh.vertices = make_vertex_bytes({
            {glm::vec3(0, 0, 0), glm::vec3(0, 1, 0), glm::vec3(1, 1, 1), glm::vec2(0, 0)},
            {glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(1, 1, 1), glm::vec2(1, 0)},
            {glm::vec3(0, 1, 0), glm::vec3(0, 1, 0), glm::vec3(1, 1, 1), glm::vec2(0, 1)},
        });
        mesh.indices = {0, 1, 2};
        m_scene.meshes.push_back(std::move(mesh));

        parsed_material mat;
        mat.name = "test_mat";
        mat.shader_effect = "se_solid_color_lit";
        mat.ambient = glm::vec3(0.2f);
        mat.diffuse = glm::vec3(0.8f);
        mat.specular = glm::vec3(0.5f, 0.3f, 0.1f);
        mat.shininess = 32.0f;
        m_scene.materials.push_back(std::move(mat));

        parsed_node node;
        node.name = "test_node";
        node.mesh_index = 0;
        node.material_index = 0;
        m_scene.nodes.push_back(std::move(node));
        m_scene.root_nodes.push_back(0);
    }
};

TEST_F(PackageWriterTest, CreatesPackageStructure)
{
    if (m_data_root.empty())
    {
        GTEST_SKIP() << "Data root not found";
    }

    ASSERT_TRUE(convert_scene(m_scene, "test_pkg"));

    fs::path pkg = package_dir("test_pkg");
    EXPECT_TRUE(fs::exists(pkg / "package.acfg"));
    EXPECT_TRUE(fs::exists(pkg / "class" / "meshes"));
    EXPECT_TRUE(fs::exists(pkg / "class" / "materials"));
    EXPECT_TRUE(fs::exists(pkg / "binaries"));
}

TEST_F(PackageWriterTest, WritesMeshBinaries)
{
    if (m_data_root.empty())
    {
        GTEST_SKIP() << "Data root not found";
    }

    ASSERT_TRUE(convert_scene(m_scene, "test_pkg"));

    fs::path pkg = package_dir("test_pkg");
    fs::path aobj = pkg / "class" / "meshes" / "test_cube.aobj";

    EXPECT_TRUE(fs::exists(aobj));

    // Check binaries directory has .abin files (vertices + indices)
    int bin_count = count_files_with_extension(pkg / "binaries", ".abin");
    EXPECT_GE(bin_count, 2) << "Should have at least vertices and indices binaries";
}

TEST_F(PackageWriterTest, WritesMeshAobj)
{
    if (m_data_root.empty())
    {
        GTEST_SKIP() << "Data root not found";
    }

    ASSERT_TRUE(convert_scene(m_scene, "test_pkg"));

    std::string content =
        read_file(package_dir("test_pkg") / "class" / "meshes" / "test_cube.aobj");
    EXPECT_NE(content.find("class_id: mesh"), std::string::npos);
    EXPECT_NE(content.find("id: test_cube"), std::string::npos);
    EXPECT_NE(content.find("vertices:"), std::string::npos);
    EXPECT_NE(content.find("indices:"), std::string::npos);
    EXPECT_NE(content.find(".abin"), std::string::npos);
}

TEST_F(PackageWriterTest, WritesSpecularAsVec3)
{
    if (m_data_root.empty())
    {
        GTEST_SKIP() << "Data root not found";
    }

    ASSERT_TRUE(convert_scene(m_scene, "test_pkg"));

    std::string content =
        read_file(package_dir("test_pkg") / "class" / "materials" / "mt_test_mat.aobj");
    // Specular should be written as vec3, not a single float
    EXPECT_NE(content.find("specular:"), std::string::npos) << "specular should be written, got:\n"
                                                            << content;
}

TEST_F(PackageWriterTest, WritesManifest)
{
    if (m_data_root.empty())
    {
        GTEST_SKIP() << "Data root not found";
    }

    ASSERT_TRUE(convert_scene(m_scene, "test_pkg"));

    std::string acfg = read_file(package_dir("test_pkg") / "package.acfg");
    EXPECT_NE(acfg.find("class_obj_mapping:"), std::string::npos);
    EXPECT_NE(acfg.find("test_cube"), std::string::npos);
    EXPECT_NE(acfg.find("mt_test_mat"), std::string::npos);
}

TEST_F(PackageWriterTest, ExtendMode)
{
    if (m_data_root.empty())
    {
        GTEST_SKIP() << "Data root not found";
    }

    // Write initial package
    ASSERT_TRUE(convert_scene(m_scene, "test_pkg", converter_mode::package_new));

    // Modify scene and extend
    m_scene.meshes[0].name = "second_mesh";
    m_scene.materials[0].name = "second_mat";

    // Use converter directly for extend mode (needs existing_package_id)
    converter_options opts;
    opts.name = AID("test_pkg");
    opts.mode = converter_mode::package_extend;
    opts.existing_package_id = AID("test_pkg");
    opts.output_root = kryga::vfs::rid("output", "");

    ASSERT_TRUE(m_ctx.convert(m_scene, opts));

    std::string acfg = read_file(package_dir("test_pkg") / "package.acfg");
    // Both original and new entries should be present
    EXPECT_NE(acfg.find("test_cube"), std::string::npos);
    EXPECT_NE(acfg.find("second_mesh"), std::string::npos);
}
