#include "engine_test_fixture.h"

#include <asset_converter/gltf_parser.h>

#include <gpu_types/gpu_vertex_types.h>

#include <iostream>
#include <vector>

namespace fs = std::filesystem;
using namespace kryga::converter;

// ============================================================================
// Discover all .glb files in the test_assets/converter directory
// ============================================================================

static std::vector<std::string>
discover_glb_files()
{
    std::vector<std::string> files;

    // Tests run from build/project_<Config>/bin → two levels up is build/
    auto dir = fs::current_path().parent_path().parent_path() / "test_assets" / "converter";
    if (!fs::exists(dir))
    {
        return files;
    }

    for (const auto& entry : fs::directory_iterator(dir))
    {
        if (entry.is_regular_file())
        {
            auto ext = entry.path().extension().string();
            for (auto& c : ext)
            {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }

            if (ext == ".glb")
            {
                files.push_back(entry.path().string());
            }
        }
    }

    std::sort(files.begin(), files.end());
    return files;
}

// Helper to get vertex count from raw bytes
static size_t
vertex_count(const std::vector<uint8_t>& bytes)
{
    return bytes.size() / sizeof(kryga::gpu::vertex_data);
}

// Helper to access vertex data from raw bytes
static const kryga::gpu::vertex_data*
vertex_data_ptr(const std::vector<uint8_t>& bytes)
{
    return reinterpret_cast<const kryga::gpu::vertex_data*>(bytes.data());
}

// ============================================================================
// Parameterized test: parse every .glb file
// ============================================================================

class KhronosGlbTest : public ::testing::TestWithParam<std::string>
{
};

TEST_P(KhronosGlbTest, ParsesWithoutCrash)
{
    parsed_scene scene;
    bool ok = parse_gltf(GetParam(), scene);

    // We expect parsing to succeed for all valid Khronos samples
    EXPECT_TRUE(ok) << "Failed to parse: " << GetParam();

    if (ok)
    {
        // Scene name should be derived from filename
        EXPECT_FALSE(scene.name.empty());

        // Should have at least some nodes
        EXPECT_GT(scene.nodes.size(), 0u) << "No nodes in: " << GetParam();
    }
}

TEST_P(KhronosGlbTest, MeshesHaveValidData)
{
    parsed_scene scene;
    if (!parse_gltf(GetParam(), scene))
    {
        GTEST_SKIP() << "Parse failed, skipping mesh validation";
    }

    for (size_t i = 0; i < scene.meshes.size(); ++i)
    {
        const auto& mesh = scene.meshes[i];
        size_t vert_count = vertex_count(mesh.vertices);

        EXPECT_FALSE(mesh.name.empty()) << "Mesh " << i << " has empty name in: " << GetParam();
        EXPECT_GT(vert_count, 0u) << "Mesh " << mesh.name << " has no vertices in: " << GetParam();
        EXPECT_GT(mesh.indices.size(), 0u)
            << "Mesh " << mesh.name << " has no indices in: " << GetParam();
        EXPECT_EQ(mesh.indices.size() % 3, 0u)
            << "Mesh " << mesh.name << " indices not multiple of 3 in: " << GetParam();

        // Verify no NaN/Inf in vertex positions
        const auto* verts = vertex_data_ptr(mesh.vertices);
        for (size_t vi = 0; vi < vert_count; ++vi)
        {
            const auto& v = verts[vi];
            EXPECT_FALSE(std::isnan(v.position.x) || std::isnan(v.position.y) ||
                         std::isnan(v.position.z))
                << "NaN position in mesh " << mesh.name << " of: " << GetParam();
            EXPECT_FALSE(std::isinf(v.position.x) || std::isinf(v.position.y) ||
                         std::isinf(v.position.z))
                << "Inf position in mesh " << mesh.name << " of: " << GetParam();
        }

        // All indices should be in bounds
        for (uint32_t idx : mesh.indices)
        {
            EXPECT_LT(idx, vert_count) << "Out of bounds index " << idx << " in mesh " << mesh.name
                                       << " (verts=" << vert_count << ") of: " << GetParam();
        }
    }
}

TEST_P(KhronosGlbTest, SceneGraphIsConsistent)
{
    parsed_scene scene;
    if (!parse_gltf(GetParam(), scene))
    {
        GTEST_SKIP() << "Parse failed, skipping graph validation";
    }

    // All root indices valid
    for (int idx : scene.root_nodes)
    {
        EXPECT_GE(idx, 0);
        EXPECT_LT(static_cast<size_t>(idx), scene.nodes.size())
            << "Invalid root node index in: " << GetParam();
    }

    // All child indices valid
    for (size_t i = 0; i < scene.nodes.size(); ++i)
    {
        const auto& node = scene.nodes[i];
        for (int child : node.children)
        {
            EXPECT_GE(child, 0);
            EXPECT_LT(static_cast<size_t>(child), scene.nodes.size())
                << "Node " << node.name << " has invalid child index in: " << GetParam();
        }

        // Mesh reference valid
        if (node.mesh_index >= 0)
        {
            EXPECT_LT(static_cast<size_t>(node.mesh_index), scene.meshes.size())
                << "Node " << node.name << " has invalid mesh_index in: " << GetParam();
        }

        // Material reference valid
        if (node.material_index >= 0)
        {
            EXPECT_LT(static_cast<size_t>(node.material_index), scene.materials.size())
                << "Node " << node.name << " has invalid material_index in: " << GetParam();
        }

        // Light reference valid
        if (node.light_index >= 0)
        {
            EXPECT_LT(static_cast<size_t>(node.light_index), scene.lights.size())
                << "Node " << node.name << " has invalid light_index in: " << GetParam();
        }

        // Camera reference valid
        if (node.camera_index >= 0)
        {
            EXPECT_LT(static_cast<size_t>(node.camera_index), scene.cameras.size())
                << "Node " << node.name << " has invalid camera_index in: " << GetParam();
        }
    }
}

// ============================================================================
// Engine serialization pipeline test - uses the engine fixture
// ============================================================================

class KhronosEnginePipelineTest : public EngineTestFixture,
                                  public ::testing::WithParamInterface<std::string>
{
};

TEST_P(KhronosEnginePipelineTest, FullPipelineDoesNotCrash)
{
    if (m_data_root.empty())
    {
        GTEST_SKIP() << "Data root not found";
    }

    parsed_scene scene;
    if (!parse_gltf(GetParam(), scene))
    {
        GTEST_SKIP() << "Parse failed for: " << GetParam();
    }

    // Derive a safe name from the file
    std::string name = std::filesystem::path(GetParam()).stem().string();
    for (auto& c : name)
    {
        if (!std::isalnum(static_cast<unsigned char>(c)))
        {
            c = '_';
        }
    }

    // Convert - just check it doesn't crash
    bool ok = convert_scene_with_level(scene, name);
    EXPECT_TRUE(ok) << "Failed to convert: " << GetParam();
}

// ============================================================================
// Register the parameterized tests
// ============================================================================

INSTANTIATE_TEST_SUITE_P(KhronosModels,
                         KhronosGlbTest,
                         ::testing::ValuesIn(discover_glb_files()),
                         [](const ::testing::TestParamInfo<std::string>& info)
                         {
                             // Use filename stem as test name
                             std::string name = std::filesystem::path(info.param).stem().string();
                             // Sanitize for gtest (only alphanumeric and underscore)
                             for (auto& c : name)
                             {
                                 if (!std::isalnum(static_cast<unsigned char>(c)))
                                 {
                                     c = '_';
                                 }
                             }
                             return name;
                         });

INSTANTIATE_TEST_SUITE_P(KhronosModels,
                         KhronosEnginePipelineTest,
                         ::testing::ValuesIn(discover_glb_files()),
                         [](const ::testing::TestParamInfo<std::string>& info)
                         {
                             std::string name = std::filesystem::path(info.param).stem().string();
                             for (auto& c : name)
                             {
                                 if (!std::isalnum(static_cast<unsigned char>(c)))
                                 {
                                     c = '_';
                                 }
                             }
                             return name;
                         });

// ============================================================================
// Summary test: report stats across all models
// ============================================================================

TEST(KhronosSummary, ReportCoverage)
{
    auto files = discover_glb_files();

    if (files.empty())
    {
        GTEST_SKIP() << "No .glb files found in test_assets/converter/";
    }

    int total = 0, parsed = 0, with_lights = 0, with_cameras = 0;
    int total_meshes = 0, total_textures = 0, total_materials = 0;
    int total_lights = 0, total_cameras = 0;

    for (const auto& f : files)
    {
        ++total;
        parsed_scene scene;
        if (parse_gltf(f, scene))
        {
            ++parsed;
            total_meshes += static_cast<int>(scene.meshes.size());
            total_textures += static_cast<int>(scene.textures.size());
            total_materials += static_cast<int>(scene.materials.size());
            total_lights += static_cast<int>(scene.lights.size());
            total_cameras += static_cast<int>(scene.cameras.size());

            if (!scene.lights.empty())
            {
                ++with_lights;
            }
            if (!scene.cameras.empty())
            {
                ++with_cameras;
            }
        }
    }

    std::cout << "\n=== Khronos Test Coverage ===\n"
              << "Files:     " << total << "\n"
              << "Parsed:    " << parsed << "/" << total << "\n"
              << "Meshes:    " << total_meshes << "\n"
              << "Textures:  " << total_textures << "\n"
              << "Materials: " << total_materials << "\n"
              << "Lights:    " << total_lights << " (in " << with_lights << " files)\n"
              << "Cameras:   " << total_cameras << " (in " << with_cameras << " files)\n"
              << "============================\n";

    EXPECT_EQ(parsed, total) << "Some files failed to parse";
}
