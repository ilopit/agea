
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

#include <gpu_types/gpu_vertex_types.h>
#include <gpu_types/gpu_camera_types.h>
#include <gpu_types/gpu_light_types.h>
#include <gpu_types/gpu_push_constants.h>

#include <global_state/global_state.h>
#include <vfs/vfs.h>

#include <utils/buffer.h>
#include <utils/dynamic_object.h>

#include <glm_unofficial/glm.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>

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
                save_png(diff_path, result.diff_image.data(), width, height);
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
    void
    SetUp() override
    {
        render::render_device::construct_params rdc;
        rdc.headless = true;

        auto device = glob::glob_state().get_render_device();
        ASSERT_TRUE(device->construct(rdc));

        state_mutator__vulkan_render_loader::set(glob::glob_state());

        glob::glob_state().getr_vulkan_render().init(TEST_WIDTH, TEST_HEIGHT,
                                                     render_mode::instanced, true);

        auto extent = VkExtent3D{TEST_WIDTH, TEST_HEIGHT, 1};

        auto img_ci = vk_utils::make_image_create_info(
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, extent);

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
        glob::glob_state().get_render_device()->destruct();
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
    void
    SetUp() override
    {
        render::render_device::construct_params rdc;
        rdc.headless = true;

        auto device = glob::glob_state().get_render_device();
        ASSERT_TRUE(device->construct(rdc));

        state_mutator__vulkan_render_loader::set(glob::glob_state());

        // Full init (not only_rp) — sets up SSBOs, render graph, compute passes
        auto& renderer = glob::glob_state().getr_vulkan_render();
        renderer.init(TEST_WIDTH, TEST_HEIGHT, render_mode::instanced, false);

        m_main_pass = glob::glob_state().getr_vulkan_render_loader().get_render_pass(AID("main"));
    }

    void
    TearDown() override
    {
        glob::glob_state().get_vulkan_render()->deinit();
        glob::glob_state().getr_vulkan_render_loader().clear_caches();
        glob::glob_state().get_render_device()->destruct();
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

        return loader.create_mesh(id, vert_buf.make_view<gpu::vertex_data>(),
                                  idx_buf.make_view<gpu::uint>());
    }

    // Create a shader effect from the lit shader source files
    shader_effect_data*
    create_lit_shader_effect(const utils::id& id)
    {
        kryga::utils::buffer vert_buf, frag_buf;

        auto path_rp = glob::glob_state().getr_vfs().real_path(vfs::rid("data://packages/base.apkg/class/shader_effects/lit"));
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

    // Matches solid_color_material__gpu layout (generated by argen)
    struct solid_color_mat_gpu
    {
        alignas(4) uint32_t texture_indices[KGPU_MAX_TEXTURE_SLOTS];
        alignas(4) uint32_t sampler_indices[KGPU_MAX_TEXTURE_SLOTS];
        alignas(16) glm::vec3 ambient;
        alignas(16) glm::vec3 diffuse;
        alignas(16) glm::vec3 specular;
        alignas(4) float shininess;
    };

    utils::dynobj
    make_solid_color_gpu_data(const glm::vec3& ambient,
                              const glm::vec3& diffuse,
                              const glm::vec3& specular,
                              float shininess)
    {
        solid_color_mat_gpu mat_gpu{};
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
    auto red_gpu = make_solid_color_gpu_data({0.8f, 0.1f, 0.1f}, {0.8f, 0.1f, 0.1f},
                                             {1.0f, 1.0f, 1.0f}, 32.0f);
    auto blue_gpu = make_solid_color_gpu_data({0.1f, 0.1f, 0.8f}, {0.1f, 0.1f, 0.8f},
                                              {1.0f, 1.0f, 1.0f}, 32.0f);

    std::vector<texture_sampler_data> no_textures;
    auto* red_mat = loader.create_material(AID("mat_red"), AID("solid_color_material"), no_textures,
                                           *se, red_gpu);
    auto* blue_mat = loader.create_material(AID("mat_blue"), AID("solid_color_material"),
                                            no_textures, *se, blue_gpu);
    ASSERT_TRUE(red_mat);
    ASSERT_TRUE(blue_mat);

    renderer.add_material(red_mat);
    renderer.add_material(blue_mat);

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

    loader.update_object(*obj1, *red_mat, *cube1_mesh, model1, glm::transpose(glm::inverse(model1)),
                         {-1.0f, 0.0f, 0.0f});
    loader.update_object(*obj2, *blue_mat, *cube2_mesh, model2,
                         glm::transpose(glm::inverse(model2)), {1.0f, 0.0f, 0.0f});

    renderer.schedule_to_drawing(obj1);
    renderer.schedule_to_drawing(obj2);
    renderer.schedule_game_data_gpu_upload(obj1);
    renderer.schedule_game_data_gpu_upload(obj2);
    renderer.schedule_material_data_gpu_upload(red_mat);
    renderer.schedule_material_data_gpu_upload(blue_mat);

    // Create a directional light
    auto* dir_light = cache.directional_lights.alloc(AID("sun"));
    ASSERT_TRUE(dir_light);
    dir_light->gpu_data.direction = {-0.2f, -1.0f, -0.1f};
    dir_light->gpu_data.ambient = {0.3f, 0.3f, 0.3f};
    dir_light->gpu_data.diffuse = {1.0f, 1.0f, 1.0f};
    dir_light->gpu_data.specular = {0.5f, 0.5f, 0.5f};
    renderer.schedule_directional_light_data_gpu_upload(dir_light);
    renderer.set_selected_directional_light(AID("sun"));

    // Disable directional shadows — self-shadowing without proper bias tuning
    renderer.get_shadow_distance() = 0.0f;

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

    // Render multiple frames so shadow maps and frame state stabilize
    for (int i = 0; i < 3; ++i) renderer.draw_headless();

    compare("lit_cubes", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}
