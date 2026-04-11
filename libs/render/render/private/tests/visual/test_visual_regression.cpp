
#include "visual_readback.h"

#include <gtest/gtest.h>

#include <render/utils/image_compare.h>

#include <vulkan_render/vulkan_render_loader.h>
#include <vulkan_render/vulkan_render_device.h>
#include <vulkan_render/kryga_render.h>
#include <vulkan_render/types/vulkan_render_pass.h>
#include <vulkan_render/types/vulkan_render_pass_builder.h>
#include <vulkan_render/types/vulkan_shader_effect_data.h>
#include <vulkan_render/utils/vulkan_image.h>
#include <vulkan_render/utils/vulkan_initializers.h>
#include <vulkan_render/bake/lightmap_baker.h>
#include <vulkan_render/lightmap_atlas.h>

#include <gpu_types/gpu_vertex_types.h>
#include <gpu_types/gpu_camera_types.h>
#include <gpu_types/gpu_light_types.h>
#include <gpu_types/gpu_push_constants_main.h>
#define KRG_GPU_STRUCT_ONLY
#include <gpu_types/solid_color_material__gpu.h>
#include <gpu_types/solid_color_alpha_material__gpu.h>
#undef KRG_GPU_STRUCT_ONLY

#include <global_state/global_state.h>
#include <vfs/vfs.h>
#include <vfs/rid.h>
#include <vfs/io.h>

#include <utils/buffer.h>
#include <utils/dynamic_object.h>

#include <glm_unofficial/glm.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <random>

using namespace kryga;
using namespace kryga::render;

namespace
{

constexpr uint32_t TEST_WIDTH = 512;
constexpr uint32_t TEST_HEIGHT = 512;

std::string
get_reference_dir()
{
    auto rp = glob::glob_state().getr_vfs().real_path(vfs::rid("data://test_references"));
    return APATH(rp.value()).str();
}

std::string
get_output_dir()
{
    auto rp = glob::glob_state().getr_vfs().real_path(vfs::rid("tmp://test_output"));
    auto dir = APATH(rp.value());
    std::filesystem::create_directories(dir.fs());
    return dir.str();
}

}  // namespace

// =============================================================================
// Base comparison helper
// =============================================================================
class visual_test_base : public ::testing::Test
{
protected:
    void
    compare(const std::string& test_name, render_pass& pass, uint32_t width, uint32_t height)
    {
        auto pixels = test::readback_framebuffer(pass, width, height);

        auto ref_dir = get_reference_dir();
        auto out_dir = get_output_dir();
        auto ref_path = ref_dir + "/" + test_name + ".png";
        auto actual_path = out_dir + "/" + test_name + "_actual.png";
        auto diff_path = out_dir + "/" + test_name + "_diff.png";

        if (std::getenv("UPDATE_REFERENCES"))
        {
            std::filesystem::create_directories(ref_dir);
            ASSERT_TRUE(save_png(ref_path, pixels.data(), width, height))
                << "Failed to save reference: " << ref_path;
            GTEST_SKIP() << "Updated reference: " << ref_path;
            return;
        }

        uint32_t ref_w = 0, ref_h = 0;
        auto ref_pixels = load_png(ref_path, ref_w, ref_h);
        ASSERT_FALSE(ref_pixels.empty()) << "Missing reference image: " << ref_path
                                         << "\nRun with UPDATE_REFERENCES=1 to generate.";
        ASSERT_EQ(ref_w, width);
        ASSERT_EQ(ref_h, height);

        image_compare_params params;
        params.width = width;
        params.height = height;
        params.pixel_tolerance = 1;
        params.ssim_threshold = 0.99f;
        params.generate_diff_image = true;

        auto result = compare_images(pixels.data(), ref_pixels.data(), params);

        if (!result.pixel_passed || !result.ssim_passed)
        {
            save_png(actual_path, pixels.data(), width, height);
            if (!result.diff_image.empty())
            {
                save_png(diff_path, result.diff_image.data(), width, height);
            }
        }

        EXPECT_TRUE(result.pixel_passed)
            << "Pixel diff: " << result.diff_pixel_count << "/" << result.total_pixels << " ("
            << result.diff_percentage << "%)"
            << "\n  Actual: " << actual_path << "\n  Diff:   " << diff_path;

        EXPECT_TRUE(result.ssim_passed)
            << "SSIM: " << result.ssim << " (threshold: " << params.ssim_threshold << ")";
    }
};

// =============================================================================
// Simple pass tests (clear color)
// =============================================================================
class visual_regression_test : public visual_test_base
{
public:
    static void
    SetUpTestSuite()
    {
        render::render_device::construct_params rdc;
        rdc.headless = true;

        auto device = glob::glob_state().get_render_device();
        ASSERT_TRUE(device->construct(rdc));

        state_mutator__vulkan_render_loader::set(glob::glob_state());
    }

    static void
    TearDownTestSuite()
    {
        glob::glob_state().get_render_device()->destruct();
    }

    void
    SetUp() override
    {
        auto device = glob::glob_state().get_render_device();

        glob::glob_state().getr_vulkan_render().init(
            TEST_WIDTH, TEST_HEIGHT, render_config{}, true);

        auto extent = VkExtent3D{TEST_WIDTH, TEST_HEIGHT, 1};

        auto img_ci = vk_utils::make_image_create_info(
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            extent);

        VmaAllocationCreateInfo alloc_ci = {};
        alloc_ci.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        m_color_image = std::make_shared<vk_utils::vulkan_image>(
            vk_utils::vulkan_image::create(device->get_vma_allocator_provider(), img_ci, alloc_ci));

        auto iv_ci = vk_utils::make_imageview_create_info(
            VK_FORMAT_R8G8B8A8_UNORM, m_color_image->image(), VK_IMAGE_ASPECT_COLOR_BIT);

        m_color_image_view = vk_utils::vulkan_image_view::create_shared(iv_ci);

        m_test_pass = render_pass_builder()
                          .set_color_format(VK_FORMAT_R8G8B8A8_UNORM)
                          .set_depth_format(VK_FORMAT_D32_SFLOAT)
                          .set_width_depth(TEST_WIDTH, TEST_HEIGHT)
                          .set_color_images({m_color_image_view}, {m_color_image})
                          .set_preset(render_pass_builder::presets::picking)
                          .set_enable_stencil(false)
                          .build();
    }

    void
    TearDown() override
    {
        m_test_pass.reset();
        m_color_image_view.reset();
        m_color_image.reset();

        glob::glob_state().get_vulkan_render()->deinit();
        glob::glob_state().getr_vulkan_render_loader().clear_caches();
    }

    render_pass_sptr m_test_pass;
    vk_utils::vulkan_image_sptr m_color_image;
    vk_utils::vulkan_image_view_sptr m_color_image_view;
};

TEST_F(visual_regression_test, clear_color_red)
{
    m_test_pass->set_clear_color({1.0f, 0.0f, 0.0f, 1.0f});

    auto& device = glob::glob_state().getr_render_device();
    device.immediate_submit(
        [&](VkCommandBuffer cmd)
        {
            m_test_pass->begin(cmd, 0, TEST_WIDTH, TEST_HEIGHT);
            m_test_pass->end(cmd);
        });

    compare("clear_color_red", *m_test_pass, TEST_WIDTH, TEST_HEIGHT);
}

TEST_F(visual_regression_test, clear_color_blue)
{
    m_test_pass->set_clear_color({0.0f, 0.0f, 1.0f, 1.0f});

    auto& device = glob::glob_state().getr_render_device();
    device.immediate_submit(
        [&](VkCommandBuffer cmd)
        {
            m_test_pass->begin(cmd, 0, TEST_WIDTH, TEST_HEIGHT);
            m_test_pass->end(cmd);
        });

    compare("clear_color_blue", *m_test_pass, TEST_WIDTH, TEST_HEIGHT);
}

// =============================================================================
// Full pipeline tests (meshes + lights via draw_headless)
// =============================================================================
class visual_pipeline_test : public visual_test_base
{
public:
    static void
    SetUpTestSuite()
    {
        render::render_device::construct_params rdc;
        rdc.headless = true;

        auto device = glob::glob_state().get_render_device();
        ASSERT_TRUE(device->construct(rdc));

        state_mutator__vulkan_render_loader::set(glob::glob_state());
    }

    static void
    TearDownTestSuite()
    {
        glob::glob_state().get_render_device()->destruct();
    }

    void
    SetUp() override
    {
        // Full init (not only_rp) — sets up SSBOs, render graph, compute passes
        auto& renderer = glob::glob_state().getr_vulkan_render();
        renderer.init(TEST_WIDTH, TEST_HEIGHT, render_config{}, false);

        m_main_pass = glob::glob_state().getr_vulkan_render_loader().get_render_pass(AID("main"));
    }

    void
    TearDown() override
    {
        glob::glob_state().get_vulkan_render()->deinit();
        glob::glob_state().getr_vulkan_render_loader().clear_caches();
    }

protected:
    // Create a cube mesh centered at origin with half-extent = 0.5
    mesh_data*
    create_cube_mesh(const utils::id& id, const glm::vec3& color)
    {
        // clang-format off
        std::vector<gpu::vertex_data> verts = {
            // Front face
            {{-0.5f, -0.5f,  0.5f}, { 0, 0, 1}, color, {0, 0}},
            {{ 0.5f, -0.5f,  0.5f}, { 0, 0, 1}, color, {1, 0}},
            {{ 0.5f,  0.5f,  0.5f}, { 0, 0, 1}, color, {1, 1}},
            {{-0.5f,  0.5f,  0.5f}, { 0, 0, 1}, color, {0, 1}},
            // Back face
            {{ 0.5f, -0.5f, -0.5f}, { 0, 0,-1}, color, {0, 0}},
            {{-0.5f, -0.5f, -0.5f}, { 0, 0,-1}, color, {1, 0}},
            {{-0.5f,  0.5f, -0.5f}, { 0, 0,-1}, color, {1, 1}},
            {{ 0.5f,  0.5f, -0.5f}, { 0, 0,-1}, color, {0, 1}},
            // Top face
            {{-0.5f,  0.5f,  0.5f}, { 0, 1, 0}, color, {0, 0}},
            {{ 0.5f,  0.5f,  0.5f}, { 0, 1, 0}, color, {1, 0}},
            {{ 0.5f,  0.5f, -0.5f}, { 0, 1, 0}, color, {1, 1}},
            {{-0.5f,  0.5f, -0.5f}, { 0, 1, 0}, color, {0, 1}},
            // Bottom face
            {{-0.5f, -0.5f, -0.5f}, { 0,-1, 0}, color, {0, 0}},
            {{ 0.5f, -0.5f, -0.5f}, { 0,-1, 0}, color, {1, 0}},
            {{ 0.5f, -0.5f,  0.5f}, { 0,-1, 0}, color, {1, 1}},
            {{-0.5f, -0.5f,  0.5f}, { 0,-1, 0}, color, {0, 1}},
            // Right face
            {{ 0.5f, -0.5f,  0.5f}, { 1, 0, 0}, color, {0, 0}},
            {{ 0.5f, -0.5f, -0.5f}, { 1, 0, 0}, color, {1, 0}},
            {{ 0.5f,  0.5f, -0.5f}, { 1, 0, 0}, color, {1, 1}},
            {{ 0.5f,  0.5f,  0.5f}, { 1, 0, 0}, color, {0, 1}},
            // Left face
            {{-0.5f, -0.5f, -0.5f}, {-1, 0, 0}, color, {0, 0}},
            {{-0.5f, -0.5f,  0.5f}, {-1, 0, 0}, color, {1, 0}},
            {{-0.5f,  0.5f,  0.5f}, {-1, 0, 0}, color, {1, 1}},
            {{-0.5f,  0.5f, -0.5f}, {-1, 0, 0}, color, {0, 1}},
        };
        std::vector<gpu::uint> indices = {
             0, 1, 2,  2, 3, 0,   // front
             4, 5, 6,  6, 7, 4,   // back
             8, 9,10, 10,11, 8,   // top
            12,13,14, 14,15,12,   // bottom
            16,17,18, 18,19,16,   // right
            20,21,22, 22,23,20,   // left
        };
        // clang-format on

        auto& loader = glob::glob_state().getr_vulkan_render_loader();

        kryga::utils::buffer vert_buf(verts.size() * sizeof(gpu::vertex_data));
        std::memcpy(vert_buf.data(), verts.data(), vert_buf.size());

        kryga::utils::buffer idx_buf(indices.size() * sizeof(gpu::uint));
        std::memcpy(idx_buf.data(), indices.data(), idx_buf.size());

        return loader.create_mesh(
            id, vert_buf.make_view<gpu::vertex_data>(), idx_buf.make_view<gpu::uint>());
    }

    // Create a shader effect from the lit shader source files
    shader_effect_data*
    create_lit_shader_effect(const utils::id& id)
    {
        kryga::utils::buffer vert_buf, frag_buf;

        auto path_rp = glob::glob_state().getr_vfs().real_path(
            vfs::rid("data://packages/base.apkg/class/shader_effects/lit"));
        auto path = APATH(path_rp.value());

        kryga::utils::buffer::load(path / "se_solid_color_lit.vert", vert_buf);
        kryga::utils::buffer::load(path / "se_solid_color_lit.frag", frag_buf);

        shader_effect_create_info se_ci;
        se_ci.vert_buffer = &vert_buf;
        se_ci.frag_buffer = &frag_buf;
        se_ci.cull_mode = VK_CULL_MODE_BACK_BIT;
        se_ci.width = TEST_WIDTH;
        se_ci.height = TEST_HEIGHT;

        shader_effect_data* sed = nullptr;
        m_main_pass->create_shader_effect(id, se_ci, sed);
        return sed;
    }

    utils::dynobj
    make_solid_color_gpu_data(const glm::vec3& ambient,
                              const glm::vec3& diffuse,
                              const glm::vec3& specular,
                              float shininess)
    {
        gpu::solid_color_material__gpu mat_gpu{};
        for (int i = 0; i < KGPU_MAX_TEXTURE_SLOTS; ++i)
        {
            mat_gpu.texture_indices[i] = UINT32_MAX;
            mat_gpu.sampler_indices[i] = UINT32_MAX;
        }
        mat_gpu.ambient = ambient;
        mat_gpu.diffuse = diffuse;
        mat_gpu.specular = specular;
        mat_gpu.shininess = shininess;

        utils::dynobj result;
        result.resize(sizeof(mat_gpu));
        std::memcpy(result.data(), &mat_gpu, sizeof(mat_gpu));
        return result;
    }

    utils::dynobj
    make_solid_color_alpha_gpu_data(const glm::vec3& ambient,
                                    const glm::vec3& diffuse,
                                    const glm::vec3& specular,
                                    float shininess,
                                    float opacity)
    {
        gpu::solid_color_alpha_material__gpu mat_gpu{};
        for (int i = 0; i < KGPU_MAX_TEXTURE_SLOTS; ++i)
        {
            mat_gpu.texture_indices[i] = UINT32_MAX;
            mat_gpu.sampler_indices[i] = UINT32_MAX;
        }
        mat_gpu.ambient = ambient;
        mat_gpu.diffuse = diffuse;
        mat_gpu.specular = specular;
        mat_gpu.shininess = shininess;
        mat_gpu.opacity = opacity;

        utils::dynobj result;
        result.resize(sizeof(mat_gpu));
        std::memcpy(result.data(), &mat_gpu, sizeof(mat_gpu));
        return result;
    }

    // Create a flat plane mesh on the XZ plane (y=0), centered at origin
    mesh_data*
    create_plane_mesh(const utils::id& id, const glm::vec3& color, float size = 5.0f)
    {
        float h = size * 0.5f;
        // clang-format off
        std::vector<gpu::vertex_data> verts = {
            {{-h, 0,  h}, {0, 1, 0}, color, {0, 0}},
            {{ h, 0,  h}, {0, 1, 0}, color, {1, 0}},
            {{ h, 0, -h}, {0, 1, 0}, color, {1, 1}},
            {{-h, 0, -h}, {0, 1, 0}, color, {0, 1}},
        };
        std::vector<gpu::uint> indices = {0, 1, 2, 2, 3, 0};
        // clang-format on

        auto& loader = glob::glob_state().getr_vulkan_render_loader();

        kryga::utils::buffer vert_buf(verts.size() * sizeof(gpu::vertex_data));
        std::memcpy(vert_buf.data(), verts.data(), vert_buf.size());

        kryga::utils::buffer idx_buf(indices.size() * sizeof(gpu::uint));
        std::memcpy(idx_buf.data(), indices.data(), idx_buf.size());

        return loader.create_mesh(
            id, vert_buf.make_view<gpu::vertex_data>(), idx_buf.make_view<gpu::uint>());
    }

    // Create a sphere mesh (UV sphere) for testing rounded geometry
    mesh_data*
    create_sphere_mesh(const utils::id& id,
                       const glm::vec3& color,
                       uint32_t stacks = 16,
                       uint32_t slices = 16,
                       float radius = 0.5f)
    {
        std::vector<gpu::vertex_data> verts;
        std::vector<gpu::uint> indices;

        for (uint32_t i = 0; i <= stacks; ++i)
        {
            float phi = glm::pi<float>() * float(i) / float(stacks);
            for (uint32_t j = 0; j <= slices; ++j)
            {
                float theta = 2.0f * glm::pi<float>() * float(j) / float(slices);
                glm::vec3 n = {std::sin(phi) * std::cos(theta),
                               std::cos(phi),
                               std::sin(phi) * std::sin(theta)};
                glm::vec3 p = n * radius;
                glm::vec2 uv = {float(j) / float(slices), float(i) / float(stacks)};
                verts.push_back({p, n, color, uv});
            }
        }

        for (uint32_t i = 0; i < stacks; ++i)
        {
            for (uint32_t j = 0; j < slices; ++j)
            {
                uint32_t a = i * (slices + 1) + j;
                uint32_t b = a + slices + 1;
                indices.insert(indices.end(), {a, a + 1, b, a + 1, b + 1, b});
            }
        }

        auto& loader = glob::glob_state().getr_vulkan_render_loader();

        kryga::utils::buffer vert_buf(verts.size() * sizeof(gpu::vertex_data));
        std::memcpy(vert_buf.data(), verts.data(), vert_buf.size());

        kryga::utils::buffer idx_buf(indices.size() * sizeof(gpu::uint));
        std::memcpy(idx_buf.data(), indices.data(), idx_buf.size());

        return loader.create_mesh(
            id, vert_buf.make_view<gpu::vertex_data>(), idx_buf.make_view<gpu::uint>());
    }

    // Common scene setup: camera + directional light + floor
    struct scene_base
    {
        vulkan_render_data* floor_obj = nullptr;
        vulkan_directional_light_data* sun = nullptr;
    };

    scene_base
    setup_scene(const std::string& prefix,
                const glm::vec3& eye,
                const glm::vec3& center,
                shader_effect_data* se,
                bool enable_shadows = false)
    {
        auto& renderer = glob::glob_state().getr_vulkan_render();
        auto& loader = glob::glob_state().getr_vulkan_render_loader();
        auto& cache = renderer.get_cache();

        // Camera
        gpu::camera_data cam;
        cam.projection = glm::perspective(
            glm::radians(60.0f), float(TEST_WIDTH) / float(TEST_HEIGHT), 0.1f, 256.0f);
        cam.projection[1][1] *= -1.0f;
        cam.view = glm::lookAt(eye, center, glm::vec3(0, 1, 0));
        cam.inv_projection = glm::inverse(cam.projection);
        cam.position = eye;
        renderer.set_camera(cam);

        // Directional light
        scene_base scene;
        auto sun_id = AID((prefix + "_sun").c_str());
        scene.sun = cache.directional_lights.alloc(sun_id);
        scene.sun->gpu_data.direction = {-0.3f, -1.0f, -0.2f};
        scene.sun->gpu_data.ambient = {0.15f, 0.15f, 0.15f};
        scene.sun->gpu_data.diffuse = {1.0f, 1.0f, 1.0f};
        scene.sun->gpu_data.specular = {0.5f, 0.5f, 0.5f};
        renderer.schd_add_light(scene.sun);
        renderer.set_selected_directional_light(sun_id);

        renderer.get_render_config().shadows.distance = enable_shadows ? 50.0f : 0.0f;

        // Floor
        auto floor_mesh_id = AID((prefix + "_floor_mesh").c_str());
        auto floor_mat_id = AID((prefix + "_mat_floor").c_str());
        auto floor_obj_id = AID((prefix + "_floor").c_str());

        auto* floor_mesh = create_plane_mesh(floor_mesh_id, {0.6f, 0.6f, 0.6f}, 10.0f);
        auto floor_gpu = make_solid_color_gpu_data(
            {0.3f, 0.3f, 0.3f}, {0.6f, 0.6f, 0.6f}, {0.2f, 0.2f, 0.2f}, 8.0f);
        std::vector<texture_sampler_data> no_tex;
        auto* floor_mat = loader.create_material(
            floor_mat_id, AID("solid_color_material"), no_tex, *se, floor_gpu);
        renderer.schd_add_material(floor_mat);

        auto model = glm::translate(glm::mat4(1.0f), glm::vec3(0, -1, 0));
        scene.floor_obj = cache.objects.alloc(floor_obj_id);
        loader.update_object(*scene.floor_obj,
                             *floor_mat,
                             *floor_mesh,
                             model,
                             glm::transpose(glm::inverse(model)),
                             {0, -1, 0});
        renderer.schd_add_object(scene.floor_obj);

        return scene;
    }

    // Create an unlit shader effect (no lighting calculations)
    shader_effect_data*
    create_unlit_shader_effect(const utils::id& id)
    {
        kryga::utils::buffer vert_buf, frag_buf;

        auto& vfs = glob::glob_state().getr_vfs();
        auto se_path = vfs.real_path(vfs::rid("data", "packages/base.apkg/class/shader_effects"));
        auto path = APATH(se_path.value());

        kryga::utils::buffer::load(path / "se_solid_color.vert", vert_buf);
        kryga::utils::buffer::load(path / "se_solid_color.frag", frag_buf);

        shader_effect_create_info se_ci;
        se_ci.vert_buffer = &vert_buf;
        se_ci.frag_buffer = &frag_buf;
        se_ci.cull_mode = VK_CULL_MODE_BACK_BIT;
        se_ci.width = TEST_WIDTH;
        se_ci.height = TEST_HEIGHT;

        shader_effect_data* sed = nullptr;
        m_main_pass->create_shader_effect(id, se_ci, sed);
        return sed;
    }

    // Create an unlit shader effect with alpha blending for transparent objects
    shader_effect_data*
    create_transparent_shader_effect(const utils::id& id)
    {
        kryga::utils::buffer vert_buf, frag_buf;

        auto& vfs = glob::glob_state().getr_vfs();
        auto se_path = vfs.real_path(vfs::rid("data", "packages/base.apkg/class/shader_effects"));
        auto path = APATH(se_path.value());

        kryga::utils::buffer::load(path / "se_solid_color.vert", vert_buf);
        kryga::utils::buffer::load(path / "se_solid_color_alpha.frag", frag_buf);

        shader_effect_create_info se_ci;
        se_ci.vert_buffer = &vert_buf;
        se_ci.frag_buffer = &frag_buf;
        se_ci.cull_mode = VK_CULL_MODE_NONE;
        se_ci.width = TEST_WIDTH;
        se_ci.height = TEST_HEIGHT;
        se_ci.alpha = alpha_mode::world;

        shader_effect_data* sed = nullptr;
        m_main_pass->create_shader_effect(id, se_ci, sed);
        return sed;
    }

    render_pass* m_main_pass = nullptr;
};

TEST_F(visual_pipeline_test, empty_frame)
{
    auto& renderer = glob::glob_state().getr_vulkan_render();

    gpu::camera_data cam;
    cam.projection = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 256.0f);
    cam.view = glm::lookAt(glm::vec3(0, 0, 5), glm::vec3(0), glm::vec3(0, 1, 0));
    cam.inv_projection = glm::inverse(cam.projection);
    cam.position = {0, 0, 5};
    renderer.set_camera(cam);

    renderer.draw_headless();

    compare("empty_frame", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

TEST_F(visual_pipeline_test, lit_cubes)
{
    auto& renderer = glob::glob_state().getr_vulkan_render();
    auto& loader = glob::glob_state().getr_vulkan_render_loader();

    // Create shader effect
    auto* se = create_lit_shader_effect(AID("se_lit_test"));
    ASSERT_TRUE(se);

    // Create two materials with different colors
    // High ambient so cubes are clearly visible even without working shadow maps
    auto red_gpu = make_solid_color_gpu_data(
        {0.8f, 0.1f, 0.1f}, {0.8f, 0.1f, 0.1f}, {1.0f, 1.0f, 1.0f}, 32.0f);
    auto blue_gpu = make_solid_color_gpu_data(
        {0.1f, 0.1f, 0.8f}, {0.1f, 0.1f, 0.8f}, {1.0f, 1.0f, 1.0f}, 32.0f);

    std::vector<texture_sampler_data> no_textures;
    auto* red_mat = loader.create_material(
        AID("mat_red"), AID("solid_color_material"), no_textures, *se, red_gpu);
    auto* blue_mat = loader.create_material(
        AID("mat_blue"), AID("solid_color_material"), no_textures, *se, blue_gpu);
    ASSERT_TRUE(red_mat);
    ASSERT_TRUE(blue_mat);

    renderer.schd_add_material(red_mat);
    renderer.schd_add_material(blue_mat);

    // Create meshes
    auto* cube1_mesh = create_cube_mesh(AID("cube1_mesh"), {1, 1, 1});
    auto* cube2_mesh = create_cube_mesh(AID("cube2_mesh"), {1, 1, 1});
    ASSERT_TRUE(cube1_mesh);
    ASSERT_TRUE(cube2_mesh);

    // Create objects
    auto& cache = renderer.get_cache();
    auto* obj1 = cache.objects.alloc(AID("cube1"));
    auto* obj2 = cache.objects.alloc(AID("cube2"));
    ASSERT_TRUE(obj1);
    ASSERT_TRUE(obj2);

    // Position cube1 at (-1, 0, 0) and cube2 at (1, 0, 0)
    auto model1 = glm::translate(glm::mat4(1.0f), glm::vec3(-1.0f, 0.0f, 0.0f));
    auto model2 = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, 0.0f));

    loader.update_object(*obj1,
                         *red_mat,
                         *cube1_mesh,
                         model1,
                         glm::transpose(glm::inverse(model1)),
                         {-1.0f, 0.0f, 0.0f});
    loader.update_object(*obj2,
                         *blue_mat,
                         *cube2_mesh,
                         model2,
                         glm::transpose(glm::inverse(model2)),
                         {1.0f, 0.0f, 0.0f});

    renderer.schd_add_object(obj1);
    renderer.schd_add_object(obj2);

    // Create a directional light
    auto* dir_light = cache.directional_lights.alloc(AID("sun"));
    ASSERT_TRUE(dir_light);
    dir_light->gpu_data.direction = {-0.2f, -1.0f, -0.1f};
    dir_light->gpu_data.ambient = {0.3f, 0.3f, 0.3f};
    dir_light->gpu_data.diffuse = {1.0f, 1.0f, 1.0f};
    dir_light->gpu_data.specular = {0.5f, 0.5f, 0.5f};
    renderer.schd_add_light(dir_light);
    renderer.set_selected_directional_light(AID("sun"));

    // Disable directional shadows — self-shadowing without proper bias tuning
    renderer.get_render_config().shadows.distance = 0.0f;

    // Set up camera looking at origin from (0, 1, 4)
    auto eye = glm::vec3(1.5f, 1.0f, 4.0f);
    auto center = glm::vec3(0.0f, 0.0f, 0.0f);
    auto up = glm::vec3(0.0f, 1.0f, 0.0f);

    gpu::camera_data cam;
    cam.projection =
        glm::perspective(glm::radians(60.0f), float(TEST_WIDTH) / float(TEST_HEIGHT), 0.1f, 256.0f);
    cam.projection[1][1] *= -1.0f;  // Vulkan Y-flip
    cam.view = glm::lookAt(eye, center, up);
    cam.inv_projection = glm::inverse(cam.projection);
    cam.position = eye;

    renderer.set_camera(cam);

    renderer.draw_headless();
    compare("lit_cubes", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// =============================================================================
// Directional light test — single cube lit from different directions
// =============================================================================
TEST_F(visual_pipeline_test, directional_light)
{
    auto& renderer = glob::glob_state().getr_vulkan_render();
    auto& loader = glob::glob_state().getr_vulkan_render_loader();

    auto* se = create_lit_shader_effect(AID("se_dirlight"));
    ASSERT_TRUE(se);

    auto eye = glm::vec3(0, 2, 5);
    auto center = glm::vec3(0, 0, 0);
    setup_scene("dir", eye, center, se);

    // White cube in the center
    auto* mesh = create_cube_mesh(AID("cube_dir"), {1, 1, 1});
    auto gpu = make_solid_color_gpu_data(
        {0.2f, 0.2f, 0.2f}, {0.9f, 0.9f, 0.9f}, {1.0f, 1.0f, 1.0f}, 64.0f);
    std::vector<texture_sampler_data> no_tex;
    auto* mat =
        loader.create_material(AID("mat_white"), AID("solid_color_material"), no_tex, *se, gpu);
    renderer.schd_add_material(mat);

    auto model = glm::mat4(1.0f);
    auto* obj = renderer.get_cache().objects.alloc(AID("cube_dir"));
    loader.update_object(*obj, *mat, *mesh, model, glm::transpose(glm::inverse(model)), {0, 0, 0});
    renderer.schd_add_object(obj);

    renderer.draw_headless();
    compare("directional_light", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// =============================================================================
// Point light test — colored point lights illuminating spheres
// =============================================================================
TEST_F(visual_pipeline_test, point_light)
{
    auto& renderer = glob::glob_state().getr_vulkan_render();
    auto& loader = glob::glob_state().getr_vulkan_render_loader();
    auto& cache = renderer.get_cache();

    auto* se = create_lit_shader_effect(AID("se_ptlight"));
    ASSERT_TRUE(se);

    auto eye = glm::vec3(0, 3, 6);
    auto center = glm::vec3(0, 0, 0);
    setup_scene("pt", eye, center, se);

    std::vector<texture_sampler_data> no_tex;

    // White sphere
    auto* sphere_mesh = create_sphere_mesh(AID("sphere_pt"), {1, 1, 1});
    auto sphere_gpu = make_solid_color_gpu_data(
        {0.1f, 0.1f, 0.1f}, {0.8f, 0.8f, 0.8f}, {1.0f, 1.0f, 1.0f}, 64.0f);
    auto* sphere_mat = loader.create_material(
        AID("mat_sphere_pt"), AID("solid_color_material"), no_tex, *se, sphere_gpu);
    renderer.schd_add_material(sphere_mat);

    auto model = glm::mat4(1.0f);
    auto* sphere_obj = cache.objects.alloc(AID("sphere_pt"));
    loader.update_object(*sphere_obj,
                         *sphere_mat,
                         *sphere_mesh,
                         model,
                         glm::transpose(glm::inverse(model)),
                         {0, 0, 0});
    renderer.schd_add_object(sphere_obj);

    // Red point light — left
    auto* red_pl = cache.universal_lights.alloc(AID("pl_red"), light_type::point);
    red_pl->gpu_data.position = {-2.0f, 1.0f, 1.0f};
    red_pl->gpu_data.ambient = {0.05f, 0.0f, 0.0f};
    red_pl->gpu_data.diffuse = {1.0f, 0.0f, 0.0f};
    red_pl->gpu_data.specular = {1.0f, 0.3f, 0.3f};
    red_pl->gpu_data.radius = 8.0f;
    red_pl->gpu_data.type = KGPU_light_type_point;
    red_pl->gpu_data.cut_off = -1.0f;
    red_pl->gpu_data.outer_cut_off = -1.0f;
    renderer.schd_add_light(red_pl);

    // Blue point light — right
    auto* blue_pl = cache.universal_lights.alloc(AID("pl_blue"), light_type::point);
    blue_pl->gpu_data.position = {2.0f, 1.0f, 1.0f};
    blue_pl->gpu_data.ambient = {0.0f, 0.0f, 0.05f};
    blue_pl->gpu_data.diffuse = {0.0f, 0.0f, 1.0f};
    blue_pl->gpu_data.specular = {0.3f, 0.3f, 1.0f};
    blue_pl->gpu_data.radius = 8.0f;
    blue_pl->gpu_data.type = KGPU_light_type_point;
    blue_pl->gpu_data.cut_off = -1.0f;
    blue_pl->gpu_data.outer_cut_off = -1.0f;
    renderer.schd_add_light(blue_pl);

    renderer.draw_headless();
    compare("point_light", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// =============================================================================
// Spot light test — focused beam on a surface
// =============================================================================
TEST_F(visual_pipeline_test, spot_light)
{
    auto& renderer = glob::glob_state().getr_vulkan_render();
    auto& loader = glob::glob_state().getr_vulkan_render_loader();
    auto& cache = renderer.get_cache();

    auto* se = create_lit_shader_effect(AID("se_spotlight"));
    ASSERT_TRUE(se);

    auto eye = glm::vec3(0, 4, 5);
    auto center = glm::vec3(0, -1, 0);
    setup_scene("spot", eye, center, se);

    std::vector<texture_sampler_data> no_tex;

    // Small cube on the floor as target
    auto* cube_mesh = create_cube_mesh(AID("cube_spot"), {1, 1, 1});
    auto cube_gpu = make_solid_color_gpu_data(
        {0.15f, 0.15f, 0.15f}, {0.7f, 0.7f, 0.7f}, {0.5f, 0.5f, 0.5f}, 32.0f);
    auto* cube_mat = loader.create_material(
        AID("mat_spot_cube"), AID("solid_color_material"), no_tex, *se, cube_gpu);
    renderer.schd_add_material(cube_mat);

    auto model = glm::translate(glm::mat4(1.0f), glm::vec3(0, -0.5f, 0));
    auto* cube_obj = cache.objects.alloc(AID("cube_spot"));
    loader.update_object(*cube_obj,
                         *cube_mat,
                         *cube_mesh,
                         model,
                         glm::transpose(glm::inverse(model)),
                         {0, -0.5f, 0});
    renderer.schd_add_object(cube_obj);

    // Yellow spot light from above
    auto* spot = cache.universal_lights.alloc(AID("spot1"), light_type::spot);
    spot->gpu_data.position = {0.0f, 3.0f, 0.0f};
    spot->gpu_data.direction = glm::normalize(glm::vec3(0.0f, -1.0f, 0.0f));
    spot->gpu_data.ambient = {0.02f, 0.02f, 0.0f};
    spot->gpu_data.diffuse = {1.0f, 1.0f, 0.3f};
    spot->gpu_data.specular = {1.0f, 1.0f, 0.5f};
    spot->gpu_data.radius = 12.0f;
    spot->gpu_data.type = KGPU_light_type_spot;
    spot->gpu_data.cut_off = std::cos(glm::radians(15.0f));
    spot->gpu_data.outer_cut_off = std::cos(glm::radians(25.0f));
    renderer.schd_add_light(spot);

    renderer.draw_headless();
    compare("spot_light", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// =============================================================================
// Shadows test — directional shadow casting onto floor
// =============================================================================
TEST_F(visual_pipeline_test, directional_shadows)
{
    auto& renderer = glob::glob_state().getr_vulkan_render();
    auto& loader = glob::glob_state().getr_vulkan_render_loader();
    auto& cache = renderer.get_cache();

    auto* se = create_lit_shader_effect(AID("se_shadow_test"));
    ASSERT_TRUE(se);

    auto eye = glm::vec3(3, 4, 6);
    auto center = glm::vec3(0, 0, 0);
    setup_scene("shadow", eye, center, se, true);  // enable shadows

    std::vector<texture_sampler_data> no_tex;

    // Two cubes casting shadows on the floor
    auto* mesh = create_cube_mesh(AID("cube_shadow"), {1, 1, 1});
    auto gpu = make_solid_color_gpu_data(
        {0.6f, 0.1f, 0.1f}, {0.8f, 0.2f, 0.2f}, {1.0f, 1.0f, 1.0f}, 32.0f);
    auto* mat = loader.create_material(
        AID("mat_shadow_red"), AID("solid_color_material"), no_tex, *se, gpu);
    renderer.schd_add_material(mat);

    // Cube 1 — elevated left
    auto model1 = glm::translate(glm::mat4(1.0f), glm::vec3(-1.5f, 0.5f, 0.0f));
    auto* obj1 = cache.objects.alloc(AID("shadow_cube1"));
    loader.update_object(
        *obj1, *mat, *mesh, model1, glm::transpose(glm::inverse(model1)), {-1.5f, 0.5f, 0});
    renderer.schd_add_object(obj1);

    // Cube 2 �� elevated right, taller
    auto model2 = glm::translate(glm::mat4(1.0f), glm::vec3(1.5f, 1.0f, 0.0f)) *
                  glm::scale(glm::mat4(1.0f), glm::vec3(0.7f, 2.0f, 0.7f));
    auto* obj2 = cache.objects.alloc(AID("shadow_cube2"));
    loader.update_object(
        *obj2, *mat, *mesh, model2, glm::transpose(glm::inverse(model2)), {1.5f, 1.0f, 0});
    renderer.schd_add_object(obj2);

    renderer.draw_headless();
    compare("directional_shadows", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// =============================================================================
// Multiple materials test — different material properties side by side
// =============================================================================
TEST_F(visual_pipeline_test, material_properties)
{
    auto& renderer = glob::glob_state().getr_vulkan_render();
    auto& loader = glob::glob_state().getr_vulkan_render_loader();
    auto& cache = renderer.get_cache();

    auto* se = create_lit_shader_effect(AID("se_materials"));
    ASSERT_TRUE(se);

    auto eye = glm::vec3(0, 2, 7);
    auto center = glm::vec3(0, 0, 0);
    setup_scene("mat", eye, center, se);

    std::vector<texture_sampler_data> no_tex;

    // Matte red (high ambient, low specular, low shininess)
    auto matte_gpu = make_solid_color_gpu_data(
        {0.5f, 0.1f, 0.1f}, {0.7f, 0.15f, 0.15f}, {0.1f, 0.1f, 0.1f}, 4.0f);
    auto* matte_mat = loader.create_material(
        AID("mat_matte"), AID("solid_color_material"), no_tex, *se, matte_gpu);
    // Glossy green (low ambient, high specular, high shininess)
    auto glossy_gpu = make_solid_color_gpu_data(
        {0.05f, 0.2f, 0.05f}, {0.1f, 0.6f, 0.1f}, {1.0f, 1.0f, 1.0f}, 128.0f);
    auto* glossy_mat = loader.create_material(
        AID("mat_glossy"), AID("solid_color_material"), no_tex, *se, glossy_gpu);
    // Metallic blue (medium ambient, high specular matching diffuse)
    auto metal_gpu = make_solid_color_gpu_data(
        {0.05f, 0.05f, 0.15f}, {0.1f, 0.1f, 0.5f}, {0.6f, 0.6f, 1.0f}, 256.0f);
    auto* metal_mat = loader.create_material(
        AID("mat_metal"), AID("solid_color_material"), no_tex, *se, metal_gpu);

    renderer.schd_add_material(matte_mat);
    renderer.schd_add_material(glossy_mat);
    renderer.schd_add_material(metal_mat);

    auto* sphere_mesh = create_sphere_mesh(AID("sphere_mat"), {1, 1, 1});

    struct mat_entry
    {
        const char* name;
        float x;
        material_data* mat;
    };
    mat_entry entries[] = {
        {"sph_matte", -2.5f, matte_mat},
        {"sph_glossy", 0.0f, glossy_mat},
        {"sph_metal", 2.5f, metal_mat},
    };

    for (auto& e : entries)
    {
        auto model = glm::translate(glm::mat4(1.0f), glm::vec3(e.x, 0, 0));
        auto* obj = cache.objects.alloc(AID(e.name));
        loader.update_object(
            *obj, *e.mat, *sphere_mesh, model, glm::transpose(glm::inverse(model)), {e.x, 0, 0});
        renderer.schd_add_object(obj);
    }

    renderer.draw_headless();
    compare("material_properties", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// =============================================================================
// Multiple light types combined — directional + point + spot in one scene
// =============================================================================
TEST_F(visual_pipeline_test, combined_lights)
{
    auto& renderer = glob::glob_state().getr_vulkan_render();
    auto& loader = glob::glob_state().getr_vulkan_render_loader();
    auto& cache = renderer.get_cache();

    auto* se = create_lit_shader_effect(AID("se_combined"));
    ASSERT_TRUE(se);

    auto eye = glm::vec3(0, 4, 8);
    auto center = glm::vec3(0, 0, 0);
    setup_scene("comb", eye, center, se);

    std::vector<texture_sampler_data> no_tex;

    // Three spheres in a row
    auto* mesh = create_sphere_mesh(AID("sphere_comb"), {1, 1, 1});
    auto gpu = make_solid_color_gpu_data(
        {0.1f, 0.1f, 0.1f}, {0.7f, 0.7f, 0.7f}, {1.0f, 1.0f, 1.0f}, 64.0f);
    auto* mat =
        loader.create_material(AID("mat_comb"), AID("solid_color_material"), no_tex, *se, gpu);
    renderer.schd_add_material(mat);

    float positions[] = {-3.0f, 0.0f, 3.0f};
    const char* names[] = {"comb_l", "comb_c", "comb_r"};
    for (int i = 0; i < 3; ++i)
    {
        auto model = glm::translate(glm::mat4(1.0f), glm::vec3(positions[i], 0, 0));
        auto* obj = cache.objects.alloc(AID(names[i]));
        loader.update_object(
            *obj, *mat, *mesh, model, glm::transpose(glm::inverse(model)), {positions[i], 0, 0});
        renderer.schd_add_object(obj);
    }

    // Green point light near left sphere
    auto* pt = cache.universal_lights.alloc(AID("pl_green"), light_type::point);
    pt->gpu_data.position = {-3.0f, 1.5f, 2.0f};
    pt->gpu_data.ambient = {0.0f, 0.02f, 0.0f};
    pt->gpu_data.diffuse = {0.0f, 1.0f, 0.0f};
    pt->gpu_data.specular = {0.0f, 1.0f, 0.0f};
    pt->gpu_data.radius = 8.0f;
    pt->gpu_data.type = KGPU_light_type_point;
    pt->gpu_data.cut_off = -1.0f;
    pt->gpu_data.outer_cut_off = -1.0f;
    renderer.schd_add_light(pt);

    // Red spot light on right sphere from above
    auto* sp = cache.universal_lights.alloc(AID("spot_red"), light_type::spot);
    sp->gpu_data.position = {3.0f, 3.0f, 1.0f};
    sp->gpu_data.direction = glm::normalize(glm::vec3(0, -1.0f, -0.3f));
    sp->gpu_data.ambient = {0.02f, 0.0f, 0.0f};
    sp->gpu_data.diffuse = {1.0f, 0.2f, 0.1f};
    sp->gpu_data.specular = {1.0f, 0.3f, 0.2f};
    sp->gpu_data.radius = 10.0f;
    sp->gpu_data.type = KGPU_light_type_spot;
    sp->gpu_data.cut_off = std::cos(glm::radians(20.0f));
    sp->gpu_data.outer_cut_off = std::cos(glm::radians(30.0f));
    renderer.schd_add_light(sp);

    renderer.draw_headless();
    compare("combined_lights", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// =============================================================================
// Multiple objects with different transforms
// =============================================================================
TEST_F(visual_pipeline_test, object_transforms)
{
    auto& renderer = glob::glob_state().getr_vulkan_render();
    auto& loader = glob::glob_state().getr_vulkan_render_loader();
    auto& cache = renderer.get_cache();

    auto* se = create_lit_shader_effect(AID("se_transforms"));
    ASSERT_TRUE(se);

    auto eye = glm::vec3(0, 5, 10);
    auto center = glm::vec3(0, 0, 0);
    setup_scene("xform", eye, center, se);

    std::vector<texture_sampler_data> no_tex;

    auto* mesh = create_cube_mesh(AID("cube_xform"), {1, 1, 1});
    auto gpu = make_solid_color_gpu_data(
        {0.15f, 0.15f, 0.3f}, {0.3f, 0.3f, 0.8f}, {1.0f, 1.0f, 1.0f}, 32.0f);
    auto* mat =
        loader.create_material(AID("mat_xform"), AID("solid_color_material"), no_tex, *se, gpu);
    renderer.schd_add_material(mat);

    // Uniform scale
    auto m1 = glm::translate(glm::mat4(1.0f), glm::vec3(-3, 0, 0)) *
              glm::scale(glm::mat4(1.0f), glm::vec3(1.5f));
    auto* o1 = cache.objects.alloc(AID("xform_scale"));
    loader.update_object(*o1, *mat, *mesh, m1, glm::transpose(glm::inverse(m1)), {-3, 0, 0});

    // Rotation 45 degrees around Y
    auto m2 = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 0)) *
              glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(0, 1, 0));
    auto* o2 = cache.objects.alloc(AID("xform_rot"));
    loader.update_object(*o2, *mat, *mesh, m2, glm::transpose(glm::inverse(m2)), {0, 0, 0});

    // Non-uniform scale (stretched tall)
    auto m3 = glm::translate(glm::mat4(1.0f), glm::vec3(3, 0.5f, 0)) *
              glm::scale(glm::mat4(1.0f), glm::vec3(0.5f, 3.0f, 0.5f));
    auto* o3 = cache.objects.alloc(AID("xform_stretch"));
    loader.update_object(*o3, *mat, *mesh, m3, glm::transpose(glm::inverse(m3)), {3, 0.5f, 0});

    for (auto* o : {o1, o2, o3})
    {
        renderer.schd_add_object(o);
    }

    renderer.draw_headless();
    compare("object_transforms", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// =============================================================================
// Alpha blending — transparent objects over opaque geometry
// =============================================================================
TEST_F(visual_pipeline_test, alpha_blending)
{
    auto& renderer = glob::glob_state().getr_vulkan_render();
    auto& loader = glob::glob_state().getr_vulkan_render_loader();
    auto& cache = renderer.get_cache();

    auto* se_unlit = create_unlit_shader_effect(AID("se_alpha_unlit"));
    auto* se_trans = create_transparent_shader_effect(AID("se_alpha_trans"));
    ASSERT_TRUE(se_unlit);
    ASSERT_TRUE(se_trans);

    // Camera
    gpu::camera_data cam;
    cam.projection = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 256.0f);
    cam.projection[1][1] *= -1.0f;
    cam.view = glm::lookAt(glm::vec3(0, 0, 4), glm::vec3(0), glm::vec3(0, 1, 0));
    cam.inv_projection = glm::inverse(cam.projection);
    cam.position = {0, 0, 4};
    renderer.set_camera(cam);
    renderer.get_render_config().shadows.distance = 0.0f;

    std::vector<texture_sampler_data> no_tex;
    auto* white_cube = create_cube_mesh(AID("alpha_white_cube"), {1, 1, 1});
    auto* blue_cube = create_cube_mesh(AID("alpha_blue_cube"), {0, 0, 1});

    // Opaque white cube behind (z = -1), unlit
    auto white_gpu =
        make_solid_color_gpu_data({1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, 1.0f);
    auto* white_mat = loader.create_material(
        AID("alpha_mat_white"), AID("solid_color_material"), no_tex, *se_unlit, white_gpu);
    renderer.schd_add_material(white_mat);

    auto model_back = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -1));
    auto* obj_back = cache.objects.alloc(AID("alpha_white"));
    loader.update_object(*obj_back,
                         *white_mat,
                         *white_cube,
                         model_back,
                         glm::transpose(glm::inverse(model_back)),
                         {0, 0, -1});
    renderer.schd_add_object(obj_back);

    // Transparent blue cube in front (z = 1), alpha = 0.3
    auto blue_gpu = make_solid_color_alpha_gpu_data(
        {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, 1.0f, 0.3f);
    auto* blue_mat = loader.create_material(
        AID("alpha_mat_blue"), AID("solid_color_alpha_material"), no_tex, *se_trans, blue_gpu);
    renderer.schd_add_material(blue_mat);

    auto model_front = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 1));
    auto* obj_front = cache.objects.alloc(AID("alpha_blue"));
    obj_front->queue_id = "transparent";
    loader.update_object(*obj_front,
                         *blue_mat,
                         *blue_cube,
                         model_front,
                         glm::transpose(glm::inverse(model_front)),
                         {0, 0, 1});
    renderer.schd_add_object(obj_front);

    renderer.draw_headless();
    compare("alpha_blending", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// =============================================================================
// Outline rendering — objects with stencil outline
// =============================================================================
TEST_F(visual_pipeline_test, outline_rendering)
{
    auto& renderer = glob::glob_state().getr_vulkan_render();
    auto& loader = glob::glob_state().getr_vulkan_render_loader();
    auto& cache = renderer.get_cache();

    auto* se = create_lit_shader_effect(AID("se_outline_test"));
    ASSERT_TRUE(se);

    setup_scene("outline", glm::vec3(0, 2, 6), glm::vec3(0, 0, 0), se);

    std::vector<texture_sampler_data> no_tex;

    auto* mesh = create_cube_mesh(AID("outline_cube"), {1, 1, 1});

    // Left cube — no outline (reference)
    auto green_gpu = make_solid_color_gpu_data(
        {0.1f, 0.4f, 0.1f}, {0.2f, 0.7f, 0.2f}, {0.5f, 1.0f, 0.5f}, 32.0f);
    auto* green_mat = loader.create_material(
        AID("outline_mat_green"), AID("solid_color_material"), no_tex, *se, green_gpu);
    renderer.schd_add_material(green_mat);

    auto m_left = glm::translate(glm::mat4(1.0f), glm::vec3(-2, 0, 0));
    auto* obj_left = cache.objects.alloc(AID("outline_left"));
    loader.update_object(
        *obj_left, *green_mat, *mesh, m_left, glm::transpose(glm::inverse(m_left)), {-2, 0, 0});
    renderer.schd_add_object(obj_left);

    // Right cube — outlined
    auto* obj_right = cache.objects.alloc(AID("outline_right"));
    obj_right->outlined = true;
    auto m_right = glm::translate(glm::mat4(1.0f), glm::vec3(2, 0, 0));
    loader.update_object(
        *obj_right, *green_mat, *mesh, m_right, glm::transpose(glm::inverse(m_right)), {2, 0, 0});
    renderer.schd_add_object(obj_right);

    renderer.draw_headless();
    compare("outline_rendering", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// =============================================================================
// Unlit shader — flat color without lighting calculations
// =============================================================================
TEST_F(visual_pipeline_test, unlit_shader)
{
    auto& renderer = glob::glob_state().getr_vulkan_render();
    auto& loader = glob::glob_state().getr_vulkan_render_loader();
    auto& cache = renderer.get_cache();

    auto* se_lit = create_lit_shader_effect(AID("se_unlit_ref"));
    auto* se_unlit = create_unlit_shader_effect(AID("se_unlit_flat"));
    ASSERT_TRUE(se_lit);
    ASSERT_TRUE(se_unlit);

    // Camera but no setup_scene — we want to control the light precisely
    gpu::camera_data cam;
    cam.projection =
        glm::perspective(glm::radians(60.0f), float(TEST_WIDTH) / float(TEST_HEIGHT), 0.1f, 256.0f);
    cam.projection[1][1] *= -1.0f;
    cam.view = glm::lookAt(glm::vec3(0, 2, 5), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    cam.inv_projection = glm::inverse(cam.projection);
    cam.position = {0, 2, 5};
    renderer.set_camera(cam);

    // Directional light — dim, to show contrast between lit and unlit
    auto* sun = cache.directional_lights.alloc(AID("unlit_sun"));
    sun->gpu_data.direction = {-0.5f, -1.0f, -0.3f};
    sun->gpu_data.ambient = {0.05f, 0.05f, 0.05f};
    sun->gpu_data.diffuse = {0.4f, 0.4f, 0.4f};
    sun->gpu_data.specular = {0.2f, 0.2f, 0.2f};
    renderer.schd_add_light(sun);
    renderer.set_selected_directional_light(AID("unlit_sun"));
    renderer.get_render_config().shadows.distance = 0.0f;

    std::vector<texture_sampler_data> no_tex;

    auto* mesh = create_sphere_mesh(AID("unlit_sphere"), {1, 1, 1});

    // Left — lit sphere (should show shading)
    auto lit_gpu = make_solid_color_gpu_data(
        {0.2f, 0.5f, 0.2f}, {0.3f, 0.8f, 0.3f}, {1.0f, 1.0f, 1.0f}, 64.0f);
    auto* lit_mat = loader.create_material(
        AID("unlit_mat_lit"), AID("solid_color_material"), no_tex, *se_lit, lit_gpu);
    renderer.schd_add_material(lit_mat);

    auto m1 = glm::translate(glm::mat4(1.0f), glm::vec3(-1.5f, 0, 0));
    auto* obj_lit = cache.objects.alloc(AID("unlit_obj_lit"));
    loader.update_object(
        *obj_lit, *lit_mat, *mesh, m1, glm::transpose(glm::inverse(m1)), {-1.5f, 0, 0});

    // Right — unlit sphere (flat color, no shading)
    auto unlit_gpu = make_solid_color_gpu_data(
        {0.2f, 0.5f, 0.2f}, {0.3f, 0.8f, 0.3f}, {1.0f, 1.0f, 1.0f}, 64.0f);
    auto* unlit_mat = loader.create_material(
        AID("unlit_mat_flat"), AID("solid_color_material"), no_tex, *se_unlit, unlit_gpu);
    renderer.schd_add_material(unlit_mat);

    auto m2 = glm::translate(glm::mat4(1.0f), glm::vec3(1.5f, 0, 0));
    auto* obj_unlit = cache.objects.alloc(AID("unlit_obj_flat"));
    loader.update_object(
        *obj_unlit, *unlit_mat, *mesh, m2, glm::transpose(glm::inverse(m2)), {1.5f, 0, 0});

    for (auto* o : {obj_lit, obj_unlit})
    {
        renderer.schd_add_object(o);
    }

    renderer.draw_headless();
    compare("unlit_shader", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// =============================================================================
// Multiple directional lights — switching active light between frames
// =============================================================================
TEST_F(visual_pipeline_test, directional_light_switching)
{
    auto& renderer = glob::glob_state().getr_vulkan_render();
    auto& loader = glob::glob_state().getr_vulkan_render_loader();
    auto& cache = renderer.get_cache();

    auto* se = create_lit_shader_effect(AID("se_dirswitch"));
    ASSERT_TRUE(se);

    // Camera
    gpu::camera_data cam;
    cam.projection =
        glm::perspective(glm::radians(60.0f), float(TEST_WIDTH) / float(TEST_HEIGHT), 0.1f, 256.0f);
    cam.projection[1][1] *= -1.0f;
    cam.view = glm::lookAt(glm::vec3(0, 2, 5), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    cam.inv_projection = glm::inverse(cam.projection);
    cam.position = {0, 2, 5};
    renderer.set_camera(cam);
    renderer.get_render_config().shadows.distance = 0.0f;

    // Two directional lights with very different colors/directions
    auto* sun_warm = cache.directional_lights.alloc(AID("dls_warm"));
    sun_warm->gpu_data.direction = {-1.0f, -0.5f, 0.0f};
    sun_warm->gpu_data.ambient = {0.1f, 0.05f, 0.02f};
    sun_warm->gpu_data.diffuse = {1.0f, 0.6f, 0.2f};
    sun_warm->gpu_data.specular = {1.0f, 0.8f, 0.4f};
    renderer.schd_add_light(sun_warm);

    auto* sun_cool = cache.directional_lights.alloc(AID("dls_cool"));
    sun_cool->gpu_data.direction = {1.0f, -0.5f, 0.0f};
    sun_cool->gpu_data.ambient = {0.02f, 0.05f, 0.1f};
    sun_cool->gpu_data.diffuse = {0.2f, 0.5f, 1.0f};
    sun_cool->gpu_data.specular = {0.4f, 0.7f, 1.0f};
    renderer.schd_add_light(sun_cool);

    std::vector<texture_sampler_data> no_tex;

    auto* mesh = create_sphere_mesh(AID("dls_sphere"), {1, 1, 1});
    auto gpu = make_solid_color_gpu_data(
        {0.2f, 0.2f, 0.2f}, {0.8f, 0.8f, 0.8f}, {1.0f, 1.0f, 1.0f}, 64.0f);
    auto* mat =
        loader.create_material(AID("dls_mat"), AID("solid_color_material"), no_tex, *se, gpu);
    renderer.schd_add_material(mat);

    auto model = glm::mat4(1.0f);
    auto* obj = cache.objects.alloc(AID("dls_obj"));
    loader.update_object(*obj, *mat, *mesh, model, glm::transpose(glm::inverse(model)), {0, 0, 0});
    renderer.schd_add_object(obj);

    // Render with warm light first, then switch to cool for final frame
    renderer.set_selected_directional_light(AID("dls_cool"));
    renderer.draw_headless();
    compare("directional_light_cool", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);

    renderer.set_selected_directional_light(AID("dls_warm"));
    renderer.draw_headless();
    compare("directional_light_warm", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// =============================================================================
// Complex scene — all features combined
// =============================================================================
TEST_F(visual_pipeline_test, complex_scene)
{
    auto& renderer = glob::glob_state().getr_vulkan_render();
    auto& loader = glob::glob_state().getr_vulkan_render_loader();
    auto& cache = renderer.get_cache();

    auto* se_lit = create_lit_shader_effect(AID("cs_se_lit"));
    auto* se_unlit = create_unlit_shader_effect(AID("cs_se_unlit"));
    auto* se_trans = create_transparent_shader_effect(AID("cs_se_trans"));
    ASSERT_TRUE(se_lit);
    ASSERT_TRUE(se_unlit);
    ASSERT_TRUE(se_trans);

    // Camera — elevated view of a populated scene
    gpu::camera_data cam;
    cam.projection =
        glm::perspective(glm::radians(60.0f), float(TEST_WIDTH) / float(TEST_HEIGHT), 0.1f, 256.0f);
    cam.projection[1][1] *= -1.0f;
    cam.view = glm::lookAt(glm::vec3(5, 6, 10), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    cam.inv_projection = glm::inverse(cam.projection);
    cam.position = {5, 6, 10};
    renderer.set_camera(cam);

    // --- Directional light (sun) with shadows ---
    auto* sun = cache.directional_lights.alloc(AID("cs_sun"));
    sun->gpu_data.direction = {-0.4f, -1.0f, -0.3f};
    sun->gpu_data.ambient = {0.12f, 0.12f, 0.15f};
    sun->gpu_data.diffuse = {0.9f, 0.85f, 0.8f};
    sun->gpu_data.specular = {0.5f, 0.5f, 0.5f};
    renderer.schd_add_light(sun);
    renderer.set_selected_directional_light(AID("cs_sun"));
    renderer.get_render_config().shadows.distance = 50.0f;

    std::vector<texture_sampler_data> no_tex;

    // --- Floor (large matte plane) ---
    auto* floor_mesh = create_plane_mesh(AID("cs_floor_mesh"), {0.5f, 0.5f, 0.5f}, 20.0f);
    auto floor_gpu = make_solid_color_gpu_data(
        {0.25f, 0.25f, 0.25f}, {0.5f, 0.5f, 0.5f}, {0.1f, 0.1f, 0.1f}, 4.0f);
    auto* floor_mat = loader.create_material(
        AID("cs_mat_floor"), AID("solid_color_material"), no_tex, *se_lit, floor_gpu);
    renderer.schd_add_material(floor_mat);

    auto floor_m = glm::translate(glm::mat4(1.0f), glm::vec3(0, -1.5f, 0));
    auto* floor_obj = cache.objects.alloc(AID("cs_floor"));
    loader.update_object(*floor_obj,
                         *floor_mat,
                         *floor_mesh,
                         floor_m,
                         glm::transpose(glm::inverse(floor_m)),
                         {0, -1.5f, 0});
    renderer.schd_add_object(floor_obj);

    // --- Metallic red cube (opaque, casting shadow) ---
    auto* cube_mesh = create_cube_mesh(AID("cs_cube_mesh"), {1, 1, 1});
    auto red_gpu = make_solid_color_gpu_data(
        {0.3f, 0.05f, 0.05f}, {0.7f, 0.1f, 0.1f}, {1.0f, 0.4f, 0.4f}, 128.0f);
    auto* red_mat = loader.create_material(
        AID("cs_mat_red"), AID("solid_color_material"), no_tex, *se_lit, red_gpu);
    renderer.schd_add_material(red_mat);

    auto red_m = glm::translate(glm::mat4(1.0f), glm::vec3(-3, 0, 0)) *
                 glm::rotate(glm::mat4(1.0f), glm::radians(30.0f), glm::vec3(0, 1, 0));
    auto* red_obj = cache.objects.alloc(AID("cs_red_cube"));
    loader.update_object(
        *red_obj, *red_mat, *cube_mesh, red_m, glm::transpose(glm::inverse(red_m)), {-3, 0, 0});
    renderer.schd_add_object(red_obj);

    // --- Glossy green sphere (opaque, outlined) ---
    auto* sphere_mesh = create_sphere_mesh(AID("cs_sphere_mesh"), {1, 1, 1});
    auto green_gpu = make_solid_color_gpu_data(
        {0.05f, 0.2f, 0.05f}, {0.1f, 0.7f, 0.1f}, {0.8f, 1.0f, 0.8f}, 256.0f);
    auto* green_mat = loader.create_material(
        AID("cs_mat_green"), AID("solid_color_material"), no_tex, *se_lit, green_gpu);
    renderer.schd_add_material(green_mat);

    auto green_m = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 0));
    auto* green_obj = cache.objects.alloc(AID("cs_green_sphere"));
    green_obj->outlined = true;
    loader.update_object(*green_obj,
                         *green_mat,
                         *sphere_mesh,
                         green_m,
                         glm::transpose(glm::inverse(green_m)),
                         {0, 0, 0});
    renderer.schd_add_object(green_obj);

    // --- Transparent blue cube (overlapping green sphere) ---
    auto trans_gpu = make_solid_color_alpha_gpu_data(
        {0.05f, 0.05f, 0.3f}, {0.1f, 0.1f, 0.5f}, {0.5f, 0.5f, 1.0f}, 32.0f, 0.4f);
    auto* trans_mat = loader.create_material(
        AID("cs_mat_trans"), AID("solid_color_alpha_material"), no_tex, *se_trans, trans_gpu);
    renderer.schd_add_material(trans_mat);

    auto trans_m = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0, 1.5f)) *
                   glm::scale(glm::mat4(1.0f), glm::vec3(1.5f, 1.5f, 1.5f));
    auto* trans_obj = cache.objects.alloc(AID("cs_trans_cube"));
    trans_obj->queue_id = "transparent";
    loader.update_object(*trans_obj,
                         *trans_mat,
                         *cube_mesh,
                         trans_m,
                         glm::transpose(glm::inverse(trans_m)),
                         {1.0f, 0, 1.5f});
    renderer.schd_add_object(trans_obj);

    // --- Unlit marker cube (small, bright yellow, no lighting) ---
    auto unlit_gpu =
        make_solid_color_gpu_data({1.0f, 0.9f, 0.0f}, {1.0f, 0.9f, 0.0f}, {0, 0, 0}, 1.0f);
    auto* unlit_mat = loader.create_material(
        AID("cs_mat_unlit"), AID("solid_color_material"), no_tex, *se_unlit, unlit_gpu);
    renderer.schd_add_material(unlit_mat);

    auto unlit_m = glm::translate(glm::mat4(1.0f), glm::vec3(3, 1.5f, -2)) *
                   glm::scale(glm::mat4(1.0f), glm::vec3(0.3f));
    auto* unlit_obj = cache.objects.alloc(AID("cs_unlit_marker"));
    loader.update_object(*unlit_obj,
                         *unlit_mat,
                         *cube_mesh,
                         unlit_m,
                         glm::transpose(glm::inverse(unlit_m)),
                         {3, 1.5f, -2});
    renderer.schd_add_object(unlit_obj);

    // --- Tall scaled pillar (non-uniform transform, casting shadow) ---
    auto pillar_gpu = make_solid_color_gpu_data(
        {0.15f, 0.1f, 0.05f}, {0.4f, 0.3f, 0.15f}, {0.3f, 0.2f, 0.1f}, 16.0f);
    auto* pillar_mat = loader.create_material(
        AID("cs_mat_pillar"), AID("solid_color_material"), no_tex, *se_lit, pillar_gpu);
    renderer.schd_add_material(pillar_mat);

    auto pillar_m = glm::translate(glm::mat4(1.0f), glm::vec3(3, 0.5f, 0)) *
                    glm::scale(glm::mat4(1.0f), glm::vec3(0.5f, 4.0f, 0.5f));
    auto* pillar_obj = cache.objects.alloc(AID("cs_pillar"));
    loader.update_object(*pillar_obj,
                         *pillar_mat,
                         *cube_mesh,
                         pillar_m,
                         glm::transpose(glm::inverse(pillar_m)),
                         {3, 0.5f, 0});
    renderer.schd_add_object(pillar_obj);

    // --- Point light (cyan, near center) ---
    auto* pt = cache.universal_lights.alloc(AID("cs_pl"), light_type::point);
    pt->gpu_data.position = {-1.0f, 2.0f, 2.0f};
    pt->gpu_data.ambient = {0.0f, 0.02f, 0.02f};
    pt->gpu_data.diffuse = {0.0f, 0.8f, 0.8f};
    pt->gpu_data.specular = {0.0f, 1.0f, 1.0f};
    pt->gpu_data.radius = 10.0f;
    pt->gpu_data.type = KGPU_light_type_point;
    pt->gpu_data.cut_off = -1.0f;
    pt->gpu_data.outer_cut_off = -1.0f;
    renderer.schd_add_light(pt);

    // --- Spot light (magenta, highlighting the pillar) ---
    auto* sp = cache.universal_lights.alloc(AID("cs_sp"), light_type::spot);
    sp->gpu_data.position = {4.0f, 4.0f, 3.0f};
    sp->gpu_data.direction = glm::normalize(glm::vec3(-1.0f, -2.0f, -3.0f));
    sp->gpu_data.ambient = {0.02f, 0.0f, 0.02f};
    sp->gpu_data.diffuse = {0.8f, 0.1f, 0.8f};
    sp->gpu_data.specular = {1.0f, 0.3f, 1.0f};
    sp->gpu_data.radius = 12.0f;
    sp->gpu_data.type = KGPU_light_type_spot;
    sp->gpu_data.cut_off = std::cos(glm::radians(18.0f));
    sp->gpu_data.outer_cut_off = std::cos(glm::radians(28.0f));
    renderer.schd_add_light(sp);

    renderer.draw_headless();
    compare("complex_scene", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// =============================================================================
// Baked lighting regression: 100+ meshes, GPU lightmap bake, render with baked GI
// =============================================================================
TEST_F(visual_pipeline_test, baked_lighting_scene)
{
    auto& renderer = glob::glob_state().getr_vulkan_render();
    auto& loader = glob::glob_state().getr_vulkan_render_loader();
    auto& cache = renderer.get_cache();

    // --- Solid color lightmapped shader effect ---
    kryga::utils::buffer lm_vert_buf, lm_frag_buf;
    {
        auto& vfs = glob::glob_state().getr_vfs();
        auto se_path =
            vfs.real_path(vfs::rid("data", "packages/base.apkg/class/shader_effects/lit"));
        auto path = APATH(se_path.value());

        kryga::utils::buffer::load(path / "se_solid_color_lit_lightmapped.vert", lm_vert_buf);
        kryga::utils::buffer::load(path / "se_solid_color_lit_lightmapped.frag", lm_frag_buf);
    }
    shader_effect_create_info se_ci;
    se_ci.vert_buffer = &lm_vert_buf;
    se_ci.frag_buffer = &lm_frag_buf;
    se_ci.cull_mode = VK_CULL_MODE_BACK_BIT;
    se_ci.width = TEST_WIDTH;
    se_ci.height = TEST_HEIGHT;

    shader_effect_data* se_lm = nullptr;
    auto se_rc = m_main_pass->create_shader_effect(AID("bk_se_lm"), se_ci, se_lm);
    ASSERT_EQ(se_rc, result_code::ok) << "Lightmapped shader effect failed to create";
    ASSERT_TRUE(se_lm);
    ASSERT_FALSE(se_lm->m_failed_load) << "Lightmapped shader marked as failed";

    // --- Camera ---
    gpu::camera_data cam;
    cam.projection =
        glm::perspective(glm::radians(60.0f), float(TEST_WIDTH) / float(TEST_HEIGHT), 0.1f, 256.0f);
    cam.projection[1][1] *= -1.0f;
    cam.view = glm::lookAt(glm::vec3(8, 10, 12), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    cam.inv_projection = glm::inverse(cam.projection);
    cam.position = {8, 10, 12};
    renderer.set_camera(cam);

    // --- Directional light for baking ---
    gpu::directional_light_data bake_sun{};
    bake_sun.direction = {-0.4f, -1.0f, -0.3f};
    bake_sun.ambient = {0.15f, 0.1f, 0.05f};
    bake_sun.diffuse = {1.2f, 0.9f, 0.5f};
    bake_sun.specular = {0.5f, 0.5f, 0.5f};

    // Zero-contribution light in renderer so the pipeline has a valid light buffer,
    // but all illumination comes from the baked lightmap only.
    auto* null_sun = cache.directional_lights.alloc(AID("bk_null_sun"));
    null_sun->gpu_data.direction = {0, -1, 0};
    null_sun->gpu_data.ambient = {0, 0, 0};
    null_sun->gpu_data.diffuse = {0, 0, 0};
    null_sun->gpu_data.specular = {0, 0, 0};
    renderer.schd_add_light(null_sun);
    renderer.set_selected_directional_light(AID("bk_null_sun"));

    std::vector<texture_sampler_data> no_tex;

    // --- Lightmapped material ---
    auto lm_gpu = make_solid_color_gpu_data(
        {0.2f, 0.2f, 0.2f}, {0.8f, 0.8f, 0.8f}, {0.3f, 0.3f, 0.3f}, 32.0f);
    auto* lm_mat = loader.create_material(
        AID("bk_mat_lm"), AID("solid_color_material"), no_tex, *se_lm, lm_gpu);
    renderer.schd_add_material(lm_mat);

    // --- Create 400 cubes at random positions with varying scales ---
    // Fixed seed for deterministic output across runs.

    constexpr uint32_t MESH_COUNT = 400;
    constexpr uint32_t LM_RESOLUTION = 2048;
    constexpr uint32_t TILE_SIZE = 48;  // Each mesh gets 48x48 texels in the atlas

    // Lightmap atlas for packing
    lightmap_atlas atlas(LM_RESOLUTION, LM_RESOLUTION);

    // Shared cube geometry with non-overlapping UV2 per face.
    // 6 faces packed in a 3x2 grid within [0,1]: each face gets (1/3 x 1/2) cell.
    // clang-format off
    //                                                               uv2 column/row
    constexpr float cw = 1.0f / 3.0f;  // cell width
    constexpr float ch = 1.0f / 2.0f;  // cell height
    std::vector<gpu::vertex_data> cube_verts = {
        // Front face  (col 0, row 0)
        {{-0.5f, -0.5f,  0.5f}, { 0, 0, 1}, {1,1,1}, {0, 0}, {0*cw,      0*ch}},
        {{ 0.5f, -0.5f,  0.5f}, { 0, 0, 1}, {1,1,1}, {1, 0}, {0*cw + cw, 0*ch}},
        {{ 0.5f,  0.5f,  0.5f}, { 0, 0, 1}, {1,1,1}, {1, 1}, {0*cw + cw, 0*ch + ch}},
        {{-0.5f,  0.5f,  0.5f}, { 0, 0, 1}, {1,1,1}, {0, 1}, {0*cw,      0*ch + ch}},
        // Back face   (col 1, row 0)
        {{ 0.5f, -0.5f, -0.5f}, { 0, 0,-1}, {1,1,1}, {0, 0}, {1*cw,      0*ch}},
        {{-0.5f, -0.5f, -0.5f}, { 0, 0,-1}, {1,1,1}, {1, 0}, {1*cw + cw, 0*ch}},
        {{-0.5f,  0.5f, -0.5f}, { 0, 0,-1}, {1,1,1}, {1, 1}, {1*cw + cw, 0*ch + ch}},
        {{ 0.5f,  0.5f, -0.5f}, { 0, 0,-1}, {1,1,1}, {0, 1}, {1*cw,      0*ch + ch}},
        // Top face    (col 2, row 0)
        {{-0.5f,  0.5f,  0.5f}, { 0, 1, 0}, {1,1,1}, {0, 0}, {2*cw,      0*ch}},
        {{ 0.5f,  0.5f,  0.5f}, { 0, 1, 0}, {1,1,1}, {1, 0}, {2*cw + cw, 0*ch}},
        {{ 0.5f,  0.5f, -0.5f}, { 0, 1, 0}, {1,1,1}, {1, 1}, {2*cw + cw, 0*ch + ch}},
        {{-0.5f,  0.5f, -0.5f}, { 0, 1, 0}, {1,1,1}, {0, 1}, {2*cw,      0*ch + ch}},
        // Bottom face (col 0, row 1)
        {{-0.5f, -0.5f, -0.5f}, { 0,-1, 0}, {1,1,1}, {0, 0}, {0*cw,      1*ch}},
        {{ 0.5f, -0.5f, -0.5f}, { 0,-1, 0}, {1,1,1}, {1, 0}, {0*cw + cw, 1*ch}},
        {{ 0.5f, -0.5f,  0.5f}, { 0,-1, 0}, {1,1,1}, {1, 1}, {0*cw + cw, 1*ch + ch}},
        {{-0.5f, -0.5f,  0.5f}, { 0,-1, 0}, {1,1,1}, {0, 1}, {0*cw,      1*ch + ch}},
        // Right face  (col 1, row 1)
        {{ 0.5f, -0.5f,  0.5f}, { 1, 0, 0}, {1,1,1}, {0, 0}, {1*cw,      1*ch}},
        {{ 0.5f, -0.5f, -0.5f}, { 1, 0, 0}, {1,1,1}, {1, 0}, {1*cw + cw, 1*ch}},
        {{ 0.5f,  0.5f, -0.5f}, { 1, 0, 0}, {1,1,1}, {1, 1}, {1*cw + cw, 1*ch + ch}},
        {{ 0.5f,  0.5f,  0.5f}, { 1, 0, 0}, {1,1,1}, {0, 1}, {1*cw,      1*ch + ch}},
        // Left face   (col 2, row 1)
        {{-0.5f, -0.5f, -0.5f}, {-1, 0, 0}, {1,1,1}, {0, 0}, {2*cw,      1*ch}},
        {{-0.5f, -0.5f,  0.5f}, {-1, 0, 0}, {1,1,1}, {1, 0}, {2*cw + cw, 1*ch}},
        {{-0.5f,  0.5f,  0.5f}, {-1, 0, 0}, {1,1,1}, {1, 1}, {2*cw + cw, 1*ch + ch}},
        {{-0.5f,  0.5f, -0.5f}, {-1, 0, 0}, {1,1,1}, {0, 1}, {2*cw,      1*ch + ch}},
    };
    std::vector<gpu::uint> cube_indices = {
         0, 1, 2,  2, 3, 0,
         4, 5, 6,  6, 7, 4,
         8, 9,10, 10,11, 8,
        12,13,14, 14,15,12,
        16,17,18, 18,19,16,
        20,21,22, 22,23,20,
    };
    // clang-format on

    // --- Floor plane mesh (single upward face, UV2 covers full [0,1]) ---
    float fh = 12.0f;
    // clang-format off
    std::vector<gpu::vertex_data> floor_verts = {
        {{-fh, 0,  fh}, {0, 1, 0}, {0.6f, 0.6f, 0.6f}, {0, 0}, {0, 0}},
        {{ fh, 0,  fh}, {0, 1, 0}, {0.6f, 0.6f, 0.6f}, {1, 0}, {1, 0}},
        {{ fh, 0, -fh}, {0, 1, 0}, {0.6f, 0.6f, 0.6f}, {1, 1}, {1, 1}},
        {{-fh, 0, -fh}, {0, 1, 0}, {0.6f, 0.6f, 0.6f}, {0, 1}, {0, 1}},
    };
    std::vector<gpu::uint> floor_indices = {0, 1, 2, 2, 3, 0};
    // clang-format on

    // --- Pack meshes into atlas and prepare baker input ---
    lightmap_baker baker;

    struct instance_info
    {
        glm::vec3 pos;
        glm::vec2 lightmap_scale;
        glm::vec2 lightmap_offset;
        float scale;
        bool is_floor;
    };
    std::vector<instance_info> instances;

    float inv_w = 1.0f / float(LM_RESOLUTION);
    float inv_h = 1.0f / float(LM_RESOLUTION);

    // Floor gets a larger tile (128x128) for better quality on the big plane
    constexpr uint32_t FLOOR_TILE = 128;
    {
        auto tile_id = AID("bk_tile_floor");
        bool packed = atlas.allocate(tile_id, FLOOR_TILE, FLOOR_TILE);
        ASSERT_TRUE(packed);
        const auto* region = atlas.get_region(tile_id);

        glm::vec2 lm_scale = {float(region->width) * inv_w, float(region->height) * inv_h};
        glm::vec2 lm_offset = {float(region->x) * inv_w, float(region->y) * inv_h};

        instances.push_back({{0, -0.5f, 0}, lm_scale, lm_offset, 1.0f, true});

        std::vector<gpu::vertex_data> remapped = floor_verts;
        for (auto& v : remapped)
        {
            v.position += glm::vec3(0, -0.5f, 0);
            v.uv2.x = v.uv2.x * lm_scale.x + lm_offset.x;
            v.uv2.y = v.uv2.y * lm_scale.y + lm_offset.y;
        }
        baker.add_mesh(remapped.data(),
                       static_cast<uint32_t>(remapped.size()),
                       floor_indices.data(),
                       static_cast<uint32_t>(floor_indices.size()));
    }

    // Deterministic RNG for reproducible positions
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist_pos(-8.0f, 8.0f);
    std::uniform_real_distribution<float> dist_y(0.0f, 3.0f);
    std::uniform_real_distribution<float> dist_scale(0.3f, 1.2f);

    for (uint32_t i = 0; i < MESH_COUNT; ++i)
    {
        auto tile_id = AID(("bk_tile_" + std::to_string(i)).c_str());

        bool packed = atlas.allocate(tile_id, TILE_SIZE, TILE_SIZE);
        ASSERT_TRUE(packed) << "Atlas full at mesh " << i;

        const auto* region = atlas.get_region(tile_id);
        ASSERT_TRUE(region);

        float wx = dist_pos(rng);
        float wz = dist_pos(rng);
        float wy = dist_y(rng);
        float scale = dist_scale(rng);

        glm::vec2 lm_scale = {float(region->width) * inv_w, float(region->height) * inv_h};
        glm::vec2 lm_offset = {float(region->x) * inv_w, float(region->y) * inv_h};

        instances.push_back({glm::vec3(wx, wy, wz), lm_scale, lm_offset, scale, false});

        // Prepare baker input: remap UV2 from [0,1] to atlas coordinates
        // and transform positions to world space for correct baking
        std::vector<gpu::vertex_data> remapped = cube_verts;
        for (auto& v : remapped)
        {
            v.position = glm::vec3(wx, wy, wz) + v.position * scale;
            v.uv2.x = v.uv2.x * lm_scale.x + lm_offset.x;
            v.uv2.y = v.uv2.y * lm_scale.y + lm_offset.y;
        }

        baker.add_mesh(remapped.data(),
                       static_cast<uint32_t>(remapped.size()),
                       cube_indices.data(),
                       static_cast<uint32_t>(cube_indices.size()));
    }

    ASSERT_EQ(instances.size(), MESH_COUNT + 1);  // 400 cubes + 1 floor

    // --- Bake lightmap (GPU compute) ---
    bake::bake_settings bake_cfg;
    bake_cfg.resolution = LM_RESOLUTION;
    bake_cfg.samples_per_texel = 16;  // Low for test speed
    bake_cfg.bounce_count = 1;
    bake_cfg.denoise_iterations = 1;
    bake_cfg.bake_direct = true;
    bake_cfg.bake_indirect = true;
    bake_cfg.bake_ao = true;
    bake_cfg.ao_radius = 2.0f;
    bake_cfg.ao_intensity = 1.0f;
    bake_cfg.directional_lights.push_back(bake_sun);
    bake_cfg.output_lightmap = vfs::rid("tmp://baked/lightmap.bin");
    bake_cfg.output_ao = vfs::rid("tmp://baked/ao.bin");
    bake_cfg.output_png = true;

    auto bake_result = baker.bake(bake_cfg);
    ASSERT_TRUE(bake_result.success) << "Lightmap bake failed";
    ASSERT_EQ(bake_result.atlas_width, LM_RESOLUTION);

    // Discard baker to prove we load from disk
    baker.clear();

    // --- Load baked data back from disk ---
    std::vector<uint8_t> lm_loaded, ao_loaded;
    ASSERT_TRUE(vfs::load_file(vfs::rid("tmp://baked/lightmap.bin"), lm_loaded));
    ASSERT_TRUE(vfs::load_file(vfs::rid("tmp://baked/ao.bin"), ao_loaded));
    ASSERT_EQ(lm_loaded.size(), LM_RESOLUTION * LM_RESOLUTION * 8);  // RGBA16F = 8 bytes/texel
    ASSERT_EQ(ao_loaded.size(), LM_RESOLUTION * LM_RESOLUTION * 2);  // R16F = 2 bytes/texel

    kryga::utils::buffer lm_tex_buf(lm_loaded.size());
    std::memcpy(lm_tex_buf.data(), lm_loaded.data(), lm_loaded.size());

    auto* lm_texture = loader.create_texture(AID("bk_lightmap_tex"),
                                             lm_tex_buf,
                                             LM_RESOLUTION,
                                             LM_RESOLUTION,
                                             VK_FORMAT_R16G16B16A16_SFLOAT,
                                             texture_format::rgba16f);
    ASSERT_TRUE(lm_texture);
    uint32_t lm_bindless_idx = lm_texture->get_bindless_index();

    // --- Create render meshes ---
    kryga::utils::buffer vert_buf(cube_verts.size() * sizeof(gpu::vertex_data));
    std::memcpy(vert_buf.data(), cube_verts.data(), vert_buf.size());
    kryga::utils::buffer idx_buf(cube_indices.size() * sizeof(gpu::uint));
    std::memcpy(idx_buf.data(), cube_indices.data(), idx_buf.size());
    auto* cube_mesh = loader.create_mesh(AID("bk_cube_mesh"),
                                         vert_buf.make_view<gpu::vertex_data>(),
                                         idx_buf.make_view<gpu::uint>());
    ASSERT_TRUE(cube_mesh);

    kryga::utils::buffer floor_vb(floor_verts.size() * sizeof(gpu::vertex_data));
    std::memcpy(floor_vb.data(), floor_verts.data(), floor_vb.size());
    kryga::utils::buffer floor_ib(floor_indices.size() * sizeof(gpu::uint));
    std::memcpy(floor_ib.data(), floor_indices.data(), floor_ib.size());
    auto* floor_mesh = loader.create_mesh(AID("bk_floor_mesh"),
                                          floor_vb.make_view<gpu::vertex_data>(),
                                          floor_ib.make_view<gpu::uint>());
    ASSERT_TRUE(floor_mesh);

    // --- Create render objects with per-instance lightmap ---
    for (uint32_t i = 0; i < instances.size(); ++i)
    {
        auto& inst = instances[i];
        auto obj_id = AID(("bk_obj_" + std::to_string(i)).c_str());

        auto* mesh = inst.is_floor ? floor_mesh : cube_mesh;
        auto model = glm::translate(glm::mat4(1.0f), inst.pos) *
                     glm::scale(glm::mat4(1.0f), glm::vec3(inst.scale));
        auto* obj = cache.objects.alloc(obj_id);
        loader.update_object(
            *obj, *lm_mat, *mesh, model, glm::transpose(glm::inverse(model)), inst.pos);

        // Per-instance lightmap binding
        obj->gpu_data.lightmap_scale = inst.lightmap_scale;
        obj->gpu_data.lightmap_offset = inst.lightmap_offset;
        obj->gpu_data.lightmap_texture_index = lm_bindless_idx;

        renderer.schd_add_object(obj);
    }

    // Triple-buffered (FRAMES_IN_FLIGHT=3): texture registration is queued to
    // frame N, but draw_headless switches frame index first. Need 3 draws to
    // cycle back to the frame that has the pending bindless update, then one
    // more to render with the updated descriptors.
    for (int i = 0; i < 4; ++i)
    {
        renderer.draw_headless();
    }
    compare("baked_lighting_scene", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}
