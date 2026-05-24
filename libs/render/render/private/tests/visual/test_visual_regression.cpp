
#include "visual_readback.h"

#include <gtest/gtest.h>

#include <render/utils/image_compare.h>
#include <render/utils/mesh_primitives.h>

#include <vulkan_render/vulkan_render_loader.h>
#include <vulkan_render/types/vulkan_texture_data.h>
#include <vulkan_render/vulkan_render_device.h>
#include <vulkan_render/kryga_render.h>
#include <vulkan_render/render_system.h>
#include <vulkan_render/types/vulkan_render_pass.h>
#include <vulkan_render/types/vulkan_render_pass_builder.h>
#include <vulkan_render/types/vulkan_shader_effect_data.h>
#include <vulkan_render/utils/vulkan_image.h>
#include <vulkan_render/utils/vulkan_initializers.h>
#include <vulkan_render/bake/lightmap_baker.h>
#include <vulkan_render/lightmap_atlas.h>

#include <gpu_types/gpu_vertex_types.h>
#include <voronoi_fracture/voronoi_fracture.h>
#include <gpu_types/gpu_camera_types.h>
#include <gpu_types/gpu_light_types.h>
#include <gpu_types/gpu_push_constants_main.h>
#define KRG_GPU_STRUCT_ONLY
#include <gpu_types/solid_color_material__gpu.h>
#include <gpu_types/solid_color_alpha_material__gpu.h>
#include <gpu_types/toon_material__gpu.h>
#include <gpu_types/pbr_material__gpu.h>
#include <gpu_types/simple_texture_material__gpu.h>
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
#include <fstream>
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
    compare(const std::string& test_name,
            render_pass& pass,
            uint32_t width,
            uint32_t height,
            VkImageLayout src_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    {
        auto pixels = test::readback_framebuffer(pass, width, height, src_layout);

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
            << "\n  Reference: " << ref_path << "\n  Actual:    " << actual_path
            << "\n  Diff:      " << diff_path;

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
        rdc.width = TEST_WIDTH;
        rdc.height = TEST_HEIGHT;

        auto& device = glob::glob_state().getr_render().device;
        ASSERT_TRUE(device.construct(rdc));
    }

    static void
    TearDownTestSuite()
    {
        glob::glob_state().getr_render().device.destruct();
    }

    void
    SetUp() override
    {
        auto& device = glob::glob_state().getr_render().device;

        render_config cfg{};
        cfg.debug.show_grid = false;
        cfg.debug.light_wireframe = false;
        cfg.debug.light_icons = false;

        glob::glob_state().getr_render().renderer.init(TEST_WIDTH, TEST_HEIGHT, cfg, true);

        auto extent = VkExtent3D{TEST_WIDTH, TEST_HEIGHT, 1};

        auto img_ci = vk_utils::make_image_create_info(
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            extent);

        VmaAllocationCreateInfo alloc_ci = {};
        alloc_ci.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        m_color_image = std::make_shared<vk_utils::vulkan_image>(
            vk_utils::vulkan_image::create(device.get_vma_allocator_provider(), img_ci, alloc_ci));

        auto iv_ci = vk_utils::make_imageview_create_info(
            VK_FORMAT_R8G8B8A8_UNORM, m_color_image->image(), VK_IMAGE_ASPECT_COLOR_BIT);

        m_color_image_view = vk_utils::vulkan_image_view::create_shared(iv_ci);

        m_test_pass = render_pass_builder()
                          .set_color_format(VK_FORMAT_R8G8B8A8_UNORM)
                          .set_depth_format(VK_FORMAT_D32_SFLOAT)
                          .set_width_depth(TEST_WIDTH, TEST_HEIGHT)
                          .set_color_images({m_color_image_view}, {m_color_image})
                          .set_enable_stencil(false)
                          .build();
    }

    void
    TearDown() override
    {
        m_test_pass.reset();
        m_color_image_view.reset();
        m_color_image.reset();

        glob::glob_state().getr_render().renderer.deinit();
        glob::glob_state().getr_render().loader.clear_caches();
    }

    render_pass_sptr m_test_pass;
    vk_utils::vulkan_image_sptr m_color_image;
    vk_utils::vulkan_image_view_sptr m_color_image_view;
};

TEST_F(visual_regression_test, clear_color_red)
{
    m_test_pass->set_clear_color({1.0f, 0.0f, 0.0f, 1.0f});

    auto& device = glob::glob_state().getr_render().device;
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

    auto& device = glob::glob_state().getr_render().device;
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
        rdc.width = TEST_WIDTH;
        rdc.height = TEST_HEIGHT;

        auto& device = glob::glob_state().getr_render().device;
        ASSERT_TRUE(device.construct(rdc));
    }

    static void
    TearDownTestSuite()
    {
        glob::glob_state().getr_render().device.destruct();
    }

    void
    SetUp() override
    {
        // Full init (not only_rp) — sets up SSBOs, render graph, compute passes
        render_config cfg{};
        cfg.debug.show_grid = false;
        cfg.debug.light_wireframe = false;
        cfg.debug.light_icons = false;

        auto& renderer = glob::glob_state().getr_render().renderer;
        renderer.init(TEST_WIDTH, TEST_HEIGHT, cfg, false);

        m_main_pass = glob::glob_state().getr_render().loader.get_render_pass(AID("main"));
    }

    void
    TearDown() override
    {
        glob::glob_state().getr_render().renderer.deinit();
        glob::glob_state().getr_render().loader.clear_caches();
    }

    // Render graph puts swapchain in TRANSFER_SRC_OPTIMAL (headless final layout)
    void
    compare(const std::string& test_name, render_pass& pass, uint32_t width, uint32_t height)
    {
        visual_test_base::compare(
            test_name, pass, width, height, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    }

protected:
    // Tear down and re-initialize the renderer with a custom config. Needed
    // for tests that toggle init-time settings: render_scale, cascade_count,
    // grid (init-time material creation), shadow map size, cluster tile size.
    void
    reinit_with_config(const render_config& cfg)
    {
        auto& renderer = glob::glob_state().getr_render().renderer;
        renderer.deinit();
        glob::glob_state().getr_render().loader.clear_caches();
        renderer.init(TEST_WIDTH, TEST_HEIGHT, cfg, false);
        m_main_pass = glob::glob_state().getr_render().loader.get_render_pass(AID("main"));
    }

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

        auto& loader = glob::glob_state().getr_render().loader;

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

        kryga::utils::buffer::load(path / "se_lit.vert.spv", vert_buf);
        kryga::utils::buffer::load(path / "se_solid_color_lit.frag.spv", frag_buf);

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

        auto& loader = glob::glob_state().getr_render().loader;

        kryga::utils::buffer vert_buf(verts.size() * sizeof(gpu::vertex_data));
        std::memcpy(vert_buf.data(), verts.data(), vert_buf.size());

        kryga::utils::buffer idx_buf(indices.size() * sizeof(gpu::uint));
        std::memcpy(idx_buf.data(), indices.data(), idx_buf.size());

        return loader.create_mesh(
            id, vert_buf.make_view<gpu::vertex_data>(), idx_buf.make_view<gpu::uint>());
    }

    mesh_data*
    create_sphere_mesh(const utils::id& id,
                       const glm::vec3& color,
                       uint32_t stacks = 16,
                       uint32_t slices = 16,
                       float radius = 0.5f)
    {
        auto sphere = render::generate_sphere(radius, stacks, slices, color.r, color.g, color.b);

        auto& loader = glob::glob_state().getr_render().loader;

        kryga::utils::buffer vert_buf(sphere.vertices.size() * sizeof(gpu::vertex_data));
        std::memcpy(vert_buf.data(), sphere.vertices.data(), vert_buf.size());

        kryga::utils::buffer idx_buf(sphere.indices.size() * sizeof(gpu::uint));
        std::memcpy(idx_buf.data(), sphere.indices.data(), idx_buf.size());

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
        auto& renderer = glob::glob_state().getr_render().renderer;
        auto& loader = glob::glob_state().getr_render().loader;
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
        renderer.get_render_config().lighting.directional_enabled = true;
        renderer.get_render_config().lighting.local_enabled = true;
        renderer.get_render_config().lighting.baked_enabled = true;

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

        kryga::utils::buffer::load(path / "se_solid_color.vert.spv", vert_buf);
        kryga::utils::buffer::load(path / "se_solid_color.frag.spv", frag_buf);

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

        kryga::utils::buffer::load(path / "se_solid_color.vert.spv", vert_buf);
        kryga::utils::buffer::load(path / "se_solid_color_alpha.frag.spv", frag_buf);

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

    // Build a textured-lit shader effect (se_lit.vert + se_pbr_lit.frag).
    // Used by material_textured_albedo, material_pbr_textured, shader_toon.
    shader_effect_data*
    create_pbr_lit_shader_effect(const utils::id& id)
    {
        kryga::utils::buffer vert_buf, frag_buf;
        auto path_rp = glob::glob_state().getr_vfs().real_path(
            vfs::rid("data://packages/base.apkg/class/shader_effects/lit"));
        auto path = APATH(path_rp.value());
        kryga::utils::buffer::load(path / "se_lit.vert.spv", vert_buf);
        kryga::utils::buffer::load(path / "se_pbr_lit.frag.spv", frag_buf);

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

    // Build a textured-unlit shader effect (se_simple_texture.vert/.frag).
    shader_effect_data*
    create_textured_unlit_shader_effect(const utils::id& id)
    {
        kryga::utils::buffer vert_buf, frag_buf;
        auto path_rp = glob::glob_state().getr_vfs().real_path(
            vfs::rid("data://packages/base.apkg/class/shader_effects"));
        auto path = APATH(path_rp.value());
        kryga::utils::buffer::load(path / "se_simple_texture.vert.spv", vert_buf);
        kryga::utils::buffer::load(path / "se_simple_texture.frag.spv", frag_buf);

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

    // Build a procedural RGBA8 checker texture. Returns texture_data*; the
    // bindless index is reachable via tex->get_bindless_index() AFTER the
    // renderer has cycled through enough draw_headless() calls to flush the
    // pending bindless update (FRAMES_IN_FLIGHT=3, so 4 draws to be safe).
    texture_data*
    create_checker_texture(const utils::id& id, uint32_t size = 64)
    {
        kryga::utils::buffer buf(size * size * 4);
        uint8_t* p = buf.data();
        for (uint32_t y = 0; y < size; ++y)
        {
            for (uint32_t x = 0; x < size; ++x)
            {
                bool checker = ((x / 8) ^ (y / 8)) & 1;
                uint8_t r = checker ? 230 : 50;
                uint8_t g = checker ? 200 : 30;
                uint8_t b = checker ? 60 : 220;  // distinct R/B so swizzle bugs are visible
                p[(y * size + x) * 4 + 0] = r;
                p[(y * size + x) * 4 + 1] = g;
                p[(y * size + x) * 4 + 2] = b;
                p[(y * size + x) * 4 + 3] = 255;
            }
        }
        auto& loader = glob::glob_state().getr_render().loader;
        return loader.create_texture(id, buf, size, size);
    }

    // Lightmapped solid-color shader effect for baked tests.
    shader_effect_data*
    create_lightmapped_shader_effect(const utils::id& id)
    {
        kryga::utils::buffer vert_buf, frag_buf;
        auto path_rp = glob::glob_state().getr_vfs().real_path(
            vfs::rid("data://packages/base.apkg/class/shader_effects/lit"));
        auto path = APATH(path_rp.value());
        kryga::utils::buffer::load(path / "se_lit.vert.spv", vert_buf);
        kryga::utils::buffer::load(path / "se_solid_color_lit.frag.spv", frag_buf);

        shader_effect_create_info se_ci;
        se_ci.vert_buffer = &vert_buf;
        se_ci.frag_buffer = &frag_buf;
        se_ci.cull_mode = VK_CULL_MODE_BACK_BIT;
        se_ci.width = TEST_WIDTH;
        se_ci.height = TEST_HEIGHT;
        se_ci.spec_constants["ENABLE_LIGHTMAP"] = 1;

        shader_effect_data* sed = nullptr;
        m_main_pass->create_shader_effect(id, se_ci, sed);
        return sed;
    }

    // Cube vertex layout with per-face packed UV2 (3x2 grid in [0,1]). Each
    // face is inset by `pad` of its cell on each side so adjacent faces don't
    // share boundary texels — dilate fills the gutter with each face's own
    // data, eliminating the dark seam at face/face junctions in the bake.
    // Used by baked tests so each face gets its own lightmap region inside
    // an instance tile. Returns a fresh copy that the caller can transform/remap.
    std::vector<gpu::vertex_data>
    make_cube_lm_verts() const
    {
        constexpr float cw = 1.0f / 3.0f;
        constexpr float ch = 1.0f / 2.0f;
        constexpr float face_pad = 0.06f;  // 6% of each cell on each side
        constexpr float ipad_u = cw * face_pad;
        constexpr float ipad_v = ch * face_pad;
        // u(col, x) maps corner_x in {0,1} to inset UV: [col*cw + ipad_u, (col+1)*cw - ipad_u]
        auto u = [](int col, int corner_x)
        { return col * cw + ipad_u + corner_x * (cw - 2 * ipad_u); };
        auto v = [](int row, int corner_y)
        { return row * ch + ipad_v + corner_y * (ch - 2 * ipad_v); };
        // clang-format off
        return {
            // Front  (col 0, row 0)
            {{-0.5f,-0.5f, 0.5f}, { 0, 0, 1}, {1,1,1}, {0,0}, {u(0,0), v(0,0)}},
            {{ 0.5f,-0.5f, 0.5f}, { 0, 0, 1}, {1,1,1}, {1,0}, {u(0,1), v(0,0)}},
            {{ 0.5f, 0.5f, 0.5f}, { 0, 0, 1}, {1,1,1}, {1,1}, {u(0,1), v(0,1)}},
            {{-0.5f, 0.5f, 0.5f}, { 0, 0, 1}, {1,1,1}, {0,1}, {u(0,0), v(0,1)}},
            // Back   (col 1, row 0)
            {{ 0.5f,-0.5f,-0.5f}, { 0, 0,-1}, {1,1,1}, {0,0}, {u(1,0), v(0,0)}},
            {{-0.5f,-0.5f,-0.5f}, { 0, 0,-1}, {1,1,1}, {1,0}, {u(1,1), v(0,0)}},
            {{-0.5f, 0.5f,-0.5f}, { 0, 0,-1}, {1,1,1}, {1,1}, {u(1,1), v(0,1)}},
            {{ 0.5f, 0.5f,-0.5f}, { 0, 0,-1}, {1,1,1}, {0,1}, {u(1,0), v(0,1)}},
            // Top    (col 2, row 0)
            {{-0.5f, 0.5f, 0.5f}, { 0, 1, 0}, {1,1,1}, {0,0}, {u(2,0), v(0,0)}},
            {{ 0.5f, 0.5f, 0.5f}, { 0, 1, 0}, {1,1,1}, {1,0}, {u(2,1), v(0,0)}},
            {{ 0.5f, 0.5f,-0.5f}, { 0, 1, 0}, {1,1,1}, {1,1}, {u(2,1), v(0,1)}},
            {{-0.5f, 0.5f,-0.5f}, { 0, 1, 0}, {1,1,1}, {0,1}, {u(2,0), v(0,1)}},
            // Bottom (col 0, row 1)
            {{-0.5f,-0.5f,-0.5f}, { 0,-1, 0}, {1,1,1}, {0,0}, {u(0,0), v(1,0)}},
            {{ 0.5f,-0.5f,-0.5f}, { 0,-1, 0}, {1,1,1}, {1,0}, {u(0,1), v(1,0)}},
            {{ 0.5f,-0.5f, 0.5f}, { 0,-1, 0}, {1,1,1}, {1,1}, {u(0,1), v(1,1)}},
            {{-0.5f,-0.5f, 0.5f}, { 0,-1, 0}, {1,1,1}, {0,1}, {u(0,0), v(1,1)}},
            // Right  (col 1, row 1)
            {{ 0.5f,-0.5f, 0.5f}, { 1, 0, 0}, {1,1,1}, {0,0}, {u(1,0), v(1,0)}},
            {{ 0.5f,-0.5f,-0.5f}, { 1, 0, 0}, {1,1,1}, {1,0}, {u(1,1), v(1,0)}},
            {{ 0.5f, 0.5f,-0.5f}, { 1, 0, 0}, {1,1,1}, {1,1}, {u(1,1), v(1,1)}},
            {{ 0.5f, 0.5f, 0.5f}, { 1, 0, 0}, {1,1,1}, {0,1}, {u(1,0), v(1,1)}},
            // Left   (col 2, row 1)
            {{-0.5f,-0.5f,-0.5f}, {-1, 0, 0}, {1,1,1}, {0,0}, {u(2,0), v(1,0)}},
            {{-0.5f,-0.5f, 0.5f}, {-1, 0, 0}, {1,1,1}, {1,0}, {u(2,1), v(1,0)}},
            {{-0.5f, 0.5f, 0.5f}, {-1, 0, 0}, {1,1,1}, {1,1}, {u(2,1), v(1,1)}},
            {{-0.5f, 0.5f,-0.5f}, {-1, 0, 0}, {1,1,1}, {0,1}, {u(2,0), v(1,1)}},
        };
        // clang-format on
    }

    std::vector<gpu::uint>
    make_cube_lm_indices() const
    {
        return {0,  1,  2,  2,  3,  0,  4,  5,  6,  6,  7,  4,  8,  9,  10, 10, 11, 8,
                12, 13, 14, 14, 15, 12, 16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20};
    }

    // Per-instance world transform + lightmap region info.
    struct bake_instance
    {
        glm::vec3 pos{0};
        float scale = 1.0f;
        glm::vec3 color{0.7f, 0.7f, 0.7f};  // diffuse for material
        bool is_floor = false;              // floor = single quad, no rotation
        bool is_wall = false;               // wall = single quad, +Z normal
        glm::mat4 extra_rot{1.0f};          // applied before translate (cubes)
        glm::vec2 lightmap_scale{0};
        glm::vec2 lightmap_offset{0};
        utils::id mat_id;
    };

    // Compact baker scaffold for the bake_* tests. Builds one shared lightmap
    // atlas, packs each instance into it, runs the GPU bake, loads the result
    // back from disk, and emits render objects with per-instance lightmap
    // bindings. Each instance gets its own material so per-mesh diffuse colors
    // survive into the lightmapped fragment path.
    void
    run_compact_bake_test(const std::string& test_name,
                          std::vector<bake_instance>& instances,
                          const glm::vec3& cam_pos,
                          const glm::vec3& cam_target,
                          const gpu::directional_light_data& bake_sun,
                          bake::bake_settings bake_cfg)
    {
        auto& renderer = glob::glob_state().getr_render().renderer;
        auto& loader = glob::glob_state().getr_render().loader;
        auto& cache = renderer.get_cache();

        auto* se_lm = create_lightmapped_shader_effect(AID((test_name + "_se_lm").c_str()));
        ASSERT_TRUE(se_lm) << "Lightmapped shader effect failed to create";
        ASSERT_FALSE(se_lm->m_failed_load) << "Lightmapped shader marked failed";

        // Camera
        gpu::camera_data cam;
        cam.projection = glm::perspective(
            glm::radians(60.0f), float(TEST_WIDTH) / float(TEST_HEIGHT), 0.1f, 256.0f);
        cam.projection[1][1] *= -1.0f;
        cam.view = glm::lookAt(cam_pos, cam_target, glm::vec3(0, 1, 0));
        cam.inv_projection = glm::inverse(cam.projection);
        cam.position = cam_pos;
        renderer.set_camera(cam);

        // Null sun in renderer — all light comes from the baked atlas.
        auto sun_id = AID((test_name + "_null_sun").c_str());
        auto* null_sun = cache.directional_lights.alloc(sun_id);
        null_sun->gpu_data.direction = {0, -1, 0};
        null_sun->gpu_data.ambient = {0, 0, 0};
        null_sun->gpu_data.diffuse = {0, 0, 0};
        null_sun->gpu_data.specular = {0, 0, 0};
        renderer.schd_add_light(null_sun);
        renderer.set_selected_directional_light(sun_id);

        renderer.get_render_config().lighting.directional_enabled = false;
        renderer.get_render_config().lighting.local_enabled = false;
        renderer.get_render_config().lighting.baked_enabled = true;

        // Atlas — sized to fit the requested resolution.
        const uint32_t LM = bake_cfg.resolution;
        lightmap_atlas atlas(LM, LM);
        const float inv_w = 1.0f / float(LM);
        const float inv_h = 1.0f / float(LM);

        const auto base_cube_verts = make_cube_lm_verts();
        const auto cube_indices = make_cube_lm_indices();

        // Floor / wall quad — single face, UV2 covers full [0,1].
        std::vector<gpu::vertex_data> floor_verts = {
            {{-1, 0, 1}, {0, 1, 0}, {1, 1, 1}, {0, 0}, {0, 0}},
            {{1, 0, 1}, {0, 1, 0}, {1, 1, 1}, {1, 0}, {1, 0}},
            {{1, 0, -1}, {0, 1, 0}, {1, 1, 1}, {1, 1}, {1, 1}},
            {{-1, 0, -1}, {0, 1, 0}, {1, 1, 1}, {0, 1}, {0, 1}},
        };
        std::vector<gpu::uint> quad_indices = {0, 1, 2, 2, 3, 0};

        std::vector<gpu::vertex_data> wall_verts = {
            {{-1, -1, 0}, {0, 0, 1}, {1, 1, 1}, {0, 0}, {0, 0}},
            {{1, -1, 0}, {0, 0, 1}, {1, 1, 1}, {1, 0}, {1, 0}},
            {{1, 1, 0}, {0, 0, 1}, {1, 1, 1}, {1, 1}, {1, 1}},
            {{-1, 1, 0}, {0, 0, 1}, {1, 1, 1}, {0, 1}, {0, 1}},
        };

        // Pack tiles + feed transformed geometry into the baker.
        lightmap_baker baker;

        // 6% padding on each side per chart: rendered geometry samples the
        // inner 88% of the atlas region; the outer 12% (6% each side) is
        // gutter that `dilate_iterations` fills with chart values, so
        // bilinear filtering at chart boundaries no longer bleeds into the
        // neighboring chart's atlas data.
        constexpr float k_chart_padding = 0.06f;
        const float chart_scale_factor = 1.0f - 2.0f * k_chart_padding;

        for (size_t i = 0; i < instances.size(); ++i)
        {
            auto& inst = instances[i];
            const uint32_t tile = (inst.is_floor || inst.is_wall) ? 256 : 64;

            auto tile_id = AID((test_name + "_tile_" + std::to_string(i)).c_str());
            ASSERT_TRUE(atlas.allocate(tile_id, tile, tile)) << "Atlas full at " << i;
            const auto* region = atlas.get_region(tile_id);
            ASSERT_TRUE(region);

            inst.lightmap_scale = {float(region->width) * chart_scale_factor * inv_w,
                                   float(region->height) * chart_scale_factor * inv_h};
            inst.lightmap_offset = {
                (float(region->x) + float(region->width) * k_chart_padding) * inv_w,
                (float(region->y) + float(region->height) * k_chart_padding) * inv_h};

            std::vector<gpu::vertex_data> remapped;
            std::vector<gpu::uint> remapped_idx;
            if (inst.is_floor)
            {
                remapped = floor_verts;
                remapped_idx = quad_indices;
                for (auto& v : remapped)
                {
                    v.position = inst.pos + v.position * inst.scale;
                    v.uv2.x = v.uv2.x * inst.lightmap_scale.x + inst.lightmap_offset.x;
                    v.uv2.y = v.uv2.y * inst.lightmap_scale.y + inst.lightmap_offset.y;
                }
            }
            else if (inst.is_wall)
            {
                remapped = wall_verts;
                remapped_idx = quad_indices;
                for (auto& v : remapped)
                {
                    glm::vec4 p = inst.extra_rot * glm::vec4(v.position * inst.scale, 1.0f);
                    glm::vec3 n = glm::mat3(inst.extra_rot) * v.normal;
                    v.position = inst.pos + glm::vec3(p);
                    v.normal = glm::normalize(n);
                    v.uv2.x = v.uv2.x * inst.lightmap_scale.x + inst.lightmap_offset.x;
                    v.uv2.y = v.uv2.y * inst.lightmap_scale.y + inst.lightmap_offset.y;
                }
            }
            else
            {
                remapped = base_cube_verts;
                remapped_idx = cube_indices;
                for (auto& v : remapped)
                {
                    glm::vec4 p = inst.extra_rot * glm::vec4(v.position * inst.scale, 1.0f);
                    glm::vec3 n = glm::mat3(inst.extra_rot) * v.normal;
                    v.position = inst.pos + glm::vec3(p);
                    v.normal = glm::normalize(n);
                    v.uv2.x = v.uv2.x * inst.lightmap_scale.x + inst.lightmap_offset.x;
                    v.uv2.y = v.uv2.y * inst.lightmap_scale.y + inst.lightmap_offset.y;
                }
            }

            baker.add_mesh(remapped.data(),
                           static_cast<uint32_t>(remapped.size()),
                           remapped_idx.data(),
                           static_cast<uint32_t>(remapped_idx.size()));
        }

        // Bake.
        bake_cfg.directional_lights.push_back(bake_sun);
        if (bake_cfg.output_lightmap.empty())
        {
            bake_cfg.output_lightmap = vfs::rid(("tmp://" + test_name + "_lm.bin").c_str());
        }
        if (bake_cfg.output_ao.empty())
        {
            bake_cfg.output_ao = vfs::rid(("tmp://" + test_name + "_ao.bin").c_str());
        }
        bake_cfg.output_png = false;

        auto bake_result = baker.bake(bake_cfg);
        ASSERT_TRUE(bake_result.success) << "Bake failed for " << test_name;
        baker.clear();

        // Reload baked atlas from disk to mirror real loading path.
        std::vector<uint8_t> lm_loaded;
        ASSERT_TRUE(vfs::load_file(bake_cfg.output_lightmap, lm_loaded));
        ASSERT_EQ(lm_loaded.size(), LM * LM * 8u);

        kryga::utils::buffer lm_buf(lm_loaded.size());
        std::memcpy(lm_buf.data(), lm_loaded.data(), lm_loaded.size());
        auto* lm_tex = loader.create_texture(AID((test_name + "_lm_tex").c_str()),
                                             lm_buf,
                                             LM,
                                             LM,
                                             VK_FORMAT_R16G16B16A16_SFLOAT,
                                             texture_format::rgba16f);
        ASSERT_TRUE(lm_tex);
        const uint32_t lm_idx = lm_tex->get_bindless_index();

        // Build render meshes (single shared cube + quad — instances get their
        // own object/transform/lightmap region but share geometry).
        kryga::utils::buffer cube_vb(base_cube_verts.size() * sizeof(gpu::vertex_data));
        std::memcpy(cube_vb.data(), base_cube_verts.data(), cube_vb.size());
        kryga::utils::buffer cube_ib(cube_indices.size() * sizeof(gpu::uint));
        std::memcpy(cube_ib.data(), cube_indices.data(), cube_ib.size());
        auto* cube_mesh = loader.create_mesh(AID((test_name + "_cube_mesh").c_str()),
                                             cube_vb.make_view<gpu::vertex_data>(),
                                             cube_ib.make_view<gpu::uint>());
        ASSERT_TRUE(cube_mesh);

        kryga::utils::buffer floor_vb(floor_verts.size() * sizeof(gpu::vertex_data));
        std::memcpy(floor_vb.data(), floor_verts.data(), floor_vb.size());
        kryga::utils::buffer floor_ib(quad_indices.size() * sizeof(gpu::uint));
        std::memcpy(floor_ib.data(), quad_indices.data(), floor_ib.size());
        auto* floor_mesh = loader.create_mesh(AID((test_name + "_floor_mesh").c_str()),
                                              floor_vb.make_view<gpu::vertex_data>(),
                                              floor_ib.make_view<gpu::uint>());
        ASSERT_TRUE(floor_mesh);

        kryga::utils::buffer wall_vb(wall_verts.size() * sizeof(gpu::vertex_data));
        std::memcpy(wall_vb.data(), wall_verts.data(), wall_vb.size());
        auto* wall_mesh = loader.create_mesh(AID((test_name + "_wall_mesh").c_str()),
                                             wall_vb.make_view<gpu::vertex_data>(),
                                             floor_ib.make_view<gpu::uint>());
        ASSERT_TRUE(wall_mesh);

        std::vector<texture_sampler_data> no_tex;

        for (size_t i = 0; i < instances.size(); ++i)
        {
            auto& inst = instances[i];

            // Per-instance material so its diffuse modulates the baked GI.
            auto mat_gpu = make_solid_color_gpu_data({0, 0, 0}, inst.color, {0, 0, 0}, 8.0f);
            auto mat_id = AID((test_name + "_mat_" + std::to_string(i)).c_str());
            inst.mat_id = mat_id;
            auto* mat = loader.create_material(
                mat_id, AID("solid_color_material"), no_tex, *se_lm, mat_gpu);
            renderer.schd_add_material(mat);

            auto obj_id = AID((test_name + "_obj_" + std::to_string(i)).c_str());
            auto* obj = cache.objects.alloc(obj_id);

            mesh_data* mesh = inst.is_floor ? floor_mesh : inst.is_wall ? wall_mesh : cube_mesh;

            auto model = glm::translate(glm::mat4(1.0f), inst.pos) * inst.extra_rot *
                         glm::scale(glm::mat4(1.0f), glm::vec3(inst.scale));
            loader.update_object(
                *obj, *mat, *mesh, model, glm::transpose(glm::inverse(model)), inst.pos);

            obj->gpu_data.lightmap_scale = inst.lightmap_scale;
            obj->gpu_data.lightmap_offset = inst.lightmap_offset;
            obj->gpu_data.lightmap_texture_index = lm_idx;

            renderer.schd_add_object(obj);
        }

        for (int i = 0; i < 4; ++i)
        {
            renderer.draw_headless();
        }
        compare(test_name, *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
    }

    render_pass* m_main_pass = nullptr;
};

TEST_F(visual_pipeline_test, empty_frame)
{
    auto& renderer = glob::glob_state().getr_render().renderer;

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
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;

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
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;

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
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
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
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
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
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
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
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
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
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
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
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
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
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
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
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
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
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
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
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
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
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
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
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
    auto& cache = renderer.get_cache();

    // --- Solid color lightmapped shader effect ---
    kryga::utils::buffer lm_vert_buf, lm_frag_buf;
    {
        auto& vfs = glob::glob_state().getr_vfs();
        auto se_path =
            vfs.real_path(vfs::rid("data", "packages/base.apkg/class/shader_effects/lit"));
        auto path = APATH(se_path.value());

        kryga::utils::buffer::load(path / "se_lit.vert.spv", lm_vert_buf);
        kryga::utils::buffer::load(path / "se_solid_color_lit.frag.spv", lm_frag_buf);
    }
    shader_effect_create_info se_ci;
    se_ci.vert_buffer = &lm_vert_buf;
    se_ci.frag_buffer = &lm_frag_buf;
    se_ci.cull_mode = VK_CULL_MODE_BACK_BIT;
    se_ci.width = TEST_WIDTH;
    se_ci.height = TEST_HEIGHT;
    se_ci.spec_constants["ENABLE_LIGHTMAP"] = 1;

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

    // Only baked lighting — disable realtime directional and local
    renderer.get_render_config().lighting.directional_enabled = false;
    renderer.get_render_config().lighting.local_enabled = false;
    renderer.get_render_config().lighting.baked_enabled = true;

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
    // 6% per-face inset gives dilate room to fill between adjacent faces so
    // bilinear filtering at face boundaries doesn't bleed across charts and
    // produce dark seams (matches make_cube_lm_verts in the bake tests).
    // clang-format off
    constexpr float cw = 1.0f / 3.0f;  // cell width
    constexpr float ch = 1.0f / 2.0f;  // cell height
    constexpr float face_pad = 0.06f;
    constexpr float ipad_u = cw * face_pad;
    constexpr float ipad_v = ch * face_pad;
    auto u = [](int col, int corner_x)
    { return col * cw + ipad_u + corner_x * (cw - 2 * ipad_u); };
    auto v = [](int row, int corner_y)
    { return row * ch + ipad_v + corner_y * (ch - 2 * ipad_v); };
    std::vector<gpu::vertex_data> cube_verts = {
        // Front face  (col 0, row 0)
        {{-0.5f, -0.5f,  0.5f}, { 0, 0, 1}, {1,1,1}, {0, 0}, {u(0,0), v(0,0)}},
        {{ 0.5f, -0.5f,  0.5f}, { 0, 0, 1}, {1,1,1}, {1, 0}, {u(0,1), v(0,0)}},
        {{ 0.5f,  0.5f,  0.5f}, { 0, 0, 1}, {1,1,1}, {1, 1}, {u(0,1), v(0,1)}},
        {{-0.5f,  0.5f,  0.5f}, { 0, 0, 1}, {1,1,1}, {0, 1}, {u(0,0), v(0,1)}},
        // Back face   (col 1, row 0)
        {{ 0.5f, -0.5f, -0.5f}, { 0, 0,-1}, {1,1,1}, {0, 0}, {u(1,0), v(0,0)}},
        {{-0.5f, -0.5f, -0.5f}, { 0, 0,-1}, {1,1,1}, {1, 0}, {u(1,1), v(0,0)}},
        {{-0.5f,  0.5f, -0.5f}, { 0, 0,-1}, {1,1,1}, {1, 1}, {u(1,1), v(0,1)}},
        {{ 0.5f,  0.5f, -0.5f}, { 0, 0,-1}, {1,1,1}, {0, 1}, {u(1,0), v(0,1)}},
        // Top face    (col 2, row 0)
        {{-0.5f,  0.5f,  0.5f}, { 0, 1, 0}, {1,1,1}, {0, 0}, {u(2,0), v(0,0)}},
        {{ 0.5f,  0.5f,  0.5f}, { 0, 1, 0}, {1,1,1}, {1, 0}, {u(2,1), v(0,0)}},
        {{ 0.5f,  0.5f, -0.5f}, { 0, 1, 0}, {1,1,1}, {1, 1}, {u(2,1), v(0,1)}},
        {{-0.5f,  0.5f, -0.5f}, { 0, 1, 0}, {1,1,1}, {0, 1}, {u(2,0), v(0,1)}},
        // Bottom face (col 0, row 1)
        {{-0.5f, -0.5f, -0.5f}, { 0,-1, 0}, {1,1,1}, {0, 0}, {u(0,0), v(1,0)}},
        {{ 0.5f, -0.5f, -0.5f}, { 0,-1, 0}, {1,1,1}, {1, 0}, {u(0,1), v(1,0)}},
        {{ 0.5f, -0.5f,  0.5f}, { 0,-1, 0}, {1,1,1}, {1, 1}, {u(0,1), v(1,1)}},
        {{-0.5f, -0.5f,  0.5f}, { 0,-1, 0}, {1,1,1}, {0, 1}, {u(0,0), v(1,1)}},
        // Right face  (col 1, row 1)
        {{ 0.5f, -0.5f,  0.5f}, { 1, 0, 0}, {1,1,1}, {0, 0}, {u(1,0), v(1,0)}},
        {{ 0.5f, -0.5f, -0.5f}, { 1, 0, 0}, {1,1,1}, {1, 0}, {u(1,1), v(1,0)}},
        {{ 0.5f,  0.5f, -0.5f}, { 1, 0, 0}, {1,1,1}, {1, 1}, {u(1,1), v(1,1)}},
        {{ 0.5f,  0.5f,  0.5f}, { 1, 0, 0}, {1,1,1}, {0, 1}, {u(1,0), v(1,1)}},
        // Left face   (col 2, row 1)
        {{-0.5f, -0.5f, -0.5f}, {-1, 0, 0}, {1,1,1}, {0, 0}, {u(2,0), v(1,0)}},
        {{-0.5f, -0.5f,  0.5f}, {-1, 0, 0}, {1,1,1}, {1, 0}, {u(2,1), v(1,0)}},
        {{-0.5f,  0.5f,  0.5f}, {-1, 0, 0}, {1,1,1}, {1, 1}, {u(2,1), v(1,1)}},
        {{-0.5f,  0.5f, -0.5f}, {-1, 0, 0}, {1,1,1}, {0, 1}, {u(2,0), v(1,1)}},
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
    bake_cfg.samples_per_texel = 256;
    bake_cfg.bounce_count = 1;
    bake_cfg.denoise_iterations = 4;
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

// =============================================================================
// Spot light shadow — fills gap: existing spot_light test runs with shadows off
// =============================================================================
TEST_F(visual_pipeline_test, shadow_spot_basic)
{
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
    auto& cache = renderer.get_cache();

    auto* se = create_lit_shader_effect(AID("se_spot_shadow"));
    ASSERT_TRUE(se);

    auto eye = glm::vec3(0, 4, 5);
    auto center = glm::vec3(0, -1, 0);
    setup_scene("spotsh", eye, center, se, true);  // shadows on

    std::vector<texture_sampler_data> no_tex;

    auto* cube_mesh = create_cube_mesh(AID("cube_spotsh"), {1, 1, 1});
    auto cube_gpu = make_solid_color_gpu_data(
        {0.15f, 0.15f, 0.15f}, {0.7f, 0.7f, 0.7f}, {0.5f, 0.5f, 0.5f}, 32.0f);
    auto* cube_mat = loader.create_material(
        AID("mat_spotsh_cube"), AID("solid_color_material"), no_tex, *se, cube_gpu);
    renderer.schd_add_material(cube_mat);

    // Cube floats above the floor so it casts a clear shadow inside the spot cone.
    auto model = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0.5f, 0));
    auto* cube_obj = cache.objects.alloc(AID("cube_spotsh"));
    loader.update_object(
        *cube_obj, *cube_mat, *cube_mesh, model, glm::transpose(glm::inverse(model)), {0, 0.5f, 0});
    renderer.schd_add_object(cube_obj);

    // Off-axis spot from above-left so the shadow falls visibly on the floor.
    auto* spot = cache.universal_lights.alloc(AID("spot_sh"), light_type::spot);
    spot->gpu_data.position = {-1.5f, 4.0f, 1.5f};
    spot->gpu_data.direction = glm::normalize(glm::vec3(0.4f, -1.0f, -0.3f));
    spot->gpu_data.ambient = {0.0f, 0.0f, 0.0f};
    spot->gpu_data.diffuse = {1.0f, 1.0f, 0.8f};
    spot->gpu_data.specular = {1.0f, 1.0f, 0.8f};
    spot->gpu_data.radius = 12.0f;
    spot->gpu_data.type = KGPU_light_type_spot;
    spot->gpu_data.cut_off = std::cos(glm::radians(20.0f));
    spot->gpu_data.outer_cut_off = std::cos(glm::radians(30.0f));
    renderer.schd_add_light(spot);

    renderer.draw_headless();
    compare("shadow_spot_basic", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// =============================================================================
// CSM cascade coverage — cubes spanning all 4 cascade splits along Z.
// Catches PSSM split regressions and bad cascade selection.
// =============================================================================
TEST_F(visual_pipeline_test, shadow_csm_cascades)
{
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
    auto& cache = renderer.get_cache();

    auto* se = create_lit_shader_effect(AID("se_csm"));
    ASSERT_TRUE(se);

    // High elevated camera looking down a long ground strip.
    gpu::camera_data cam;
    cam.projection =
        glm::perspective(glm::radians(60.0f), float(TEST_WIDTH) / float(TEST_HEIGHT), 0.1f, 256.0f);
    cam.projection[1][1] *= -1.0f;
    cam.view = glm::lookAt(glm::vec3(0, 8, 4), glm::vec3(0, 0, -25), glm::vec3(0, 1, 0));
    cam.inv_projection = glm::inverse(cam.projection);
    cam.position = {0, 8, 4};
    renderer.set_camera(cam);

    auto* sun = cache.directional_lights.alloc(AID("csm_sun"));
    sun->gpu_data.direction = {-0.3f, -1.0f, -0.2f};
    sun->gpu_data.ambient = {0.15f, 0.15f, 0.15f};
    sun->gpu_data.diffuse = {1.0f, 1.0f, 1.0f};
    sun->gpu_data.specular = {0.5f, 0.5f, 0.5f};
    renderer.schd_add_light(sun);
    renderer.set_selected_directional_light(AID("csm_sun"));
    renderer.get_render_config().shadows.distance = 100.0f;
    renderer.get_render_config().shadows.cascade_count = 4;

    std::vector<texture_sampler_data> no_tex;

    // Long ground strip stretched along -Z.
    auto* ground = create_plane_mesh(AID("csm_ground"), {0.6f, 0.6f, 0.6f}, 60.0f);
    auto ground_gpu =
        make_solid_color_gpu_data({0.3f, 0.3f, 0.3f}, {0.6f, 0.6f, 0.6f}, {0.1f, 0.1f, 0.1f}, 8.0f);
    auto* ground_mat = loader.create_material(
        AID("csm_ground_mat"), AID("solid_color_material"), no_tex, *se, ground_gpu);
    renderer.schd_add_material(ground_mat);

    auto ground_m = glm::translate(glm::mat4(1.0f), glm::vec3(0, -1, -25));
    auto* ground_obj = cache.objects.alloc(AID("csm_ground_obj"));
    loader.update_object(*ground_obj,
                         *ground_mat,
                         *ground,
                         ground_m,
                         glm::transpose(glm::inverse(ground_m)),
                         {0, -1, -25});
    renderer.schd_add_object(ground_obj);

    // Cubes at z = -2, -10, -25, -45 — one per cascade band.
    auto* cube_mesh = create_cube_mesh(AID("csm_cube"), {1, 1, 1});
    auto cube_gpu = make_solid_color_gpu_data(
        {0.2f, 0.1f, 0.1f}, {0.7f, 0.2f, 0.2f}, {0.5f, 0.5f, 0.5f}, 32.0f);
    auto* cube_mat = loader.create_material(
        AID("csm_cube_mat"), AID("solid_color_material"), no_tex, *se, cube_gpu);
    renderer.schd_add_material(cube_mat);

    float depths[] = {-2.0f, -10.0f, -25.0f, -45.0f};
    for (int i = 0; i < 4; ++i)
    {
        auto m = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, depths[i]));
        auto* obj = cache.objects.alloc(AID(("csm_cube_" + std::to_string(i)).c_str()));
        loader.update_object(
            *obj, *cube_mat, *cube_mesh, m, glm::transpose(glm::inverse(m)), {0, 0, depths[i]});
        renderer.schd_add_object(obj);
    }

    renderer.draw_headless();
    compare("shadow_csm_cascades", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// =============================================================================
// PCF mode coverage — same shadow scene, parameterized over 5 PCF modes.
// Each mode should produce visibly different penumbra softness/noise.
// =============================================================================
class shadow_pcf_test : public visual_pipeline_test, public testing::WithParamInterface<pcf_mode>
{
public:
    static void
    SetUpTestSuite()
    {
        visual_pipeline_test::SetUpTestSuite();
    }
    static void
    TearDownTestSuite()
    {
        visual_pipeline_test::TearDownTestSuite();
    }

    static const char*
    pcf_suffix(pcf_mode m)
    {
        switch (m)
        {
        case pcf_mode::pcf_3x3:
            return "3x3";
        case pcf_mode::pcf_5x5:
            return "5x5";
        case pcf_mode::pcf_7x7:
            return "7x7";
        case pcf_mode::poisson16:
            return "poisson16";
        case pcf_mode::poisson32:
            return "poisson32";
        }
        return "unknown";
    }
};

TEST_P(shadow_pcf_test, looks_correct)
{
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
    auto& cache = renderer.get_cache();

    auto* se = create_lit_shader_effect(AID("se_pcf_test"));
    ASSERT_TRUE(se);

    // setup_scene gives us floor + sun + shadows; immediately override the
    // camera with a tight close-up onto the shadow edge so PCF penumbra
    // differences (a few texels wide) become visible at 512x512.
    setup_scene("pcf", glm::vec3(3, 4, 6), glm::vec3(0, 0, 0), se, true);
    renderer.get_render_config().shadows.pcf = GetParam();

    // Tight overhead camera framed on the right cube's shadow on the floor.
    // The cube floats (y=0..2) above floor (y=-1); sun direction (-0.3,-1,-0.2)
    // projects its shadow into the region x in [0.25, 1.55], z in [-0.95, 0.15]
    // on the floor — looking straight down so the penumbra band reads cleanly.
    gpu::camera_data cam;
    cam.projection =
        glm::perspective(glm::radians(15.0f), float(TEST_WIDTH) / float(TEST_HEIGHT), 0.1f, 256.0f);
    cam.projection[1][1] *= -1.0f;
    cam.view = glm::lookAt(
        glm::vec3(0.5f, 4.0f, 0.5f), glm::vec3(0.5f, -1.0f, -0.4f), glm::vec3(0, 0, -1));
    cam.inv_projection = glm::inverse(cam.projection);
    cam.position = {0.5f, 4.0f, 0.5f};
    renderer.set_camera(cam);

    std::vector<texture_sampler_data> no_tex;

    // Same two cubes as directional_shadows, but only the right one matters
    // for this close-up view.
    auto* mesh = create_cube_mesh(AID("cube_pcf"), {1, 1, 1});
    auto gpu = make_solid_color_gpu_data(
        {0.6f, 0.1f, 0.1f}, {0.8f, 0.2f, 0.2f}, {1.0f, 1.0f, 1.0f}, 32.0f);
    auto* mat =
        loader.create_material(AID("mat_pcf"), AID("solid_color_material"), no_tex, *se, gpu);
    renderer.schd_add_material(mat);

    auto m1 = glm::translate(glm::mat4(1.0f), glm::vec3(-1.5f, 0.5f, 0.0f));
    auto* o1 = cache.objects.alloc(AID("pcf_cube1"));
    loader.update_object(*o1, *mat, *mesh, m1, glm::transpose(glm::inverse(m1)), {-1.5f, 0.5f, 0});
    renderer.schd_add_object(o1);

    auto m2 = glm::translate(glm::mat4(1.0f), glm::vec3(1.5f, 1.0f, 0.0f)) *
              glm::scale(glm::mat4(1.0f), glm::vec3(0.7f, 2.0f, 0.7f));
    auto* o2 = cache.objects.alloc(AID("pcf_cube2"));
    loader.update_object(*o2, *mat, *mesh, m2, glm::transpose(glm::inverse(m2)), {1.5f, 1.0f, 0});
    renderer.schd_add_object(o2);

    renderer.draw_headless();
    compare(
        std::string("shadow_pcf_") + pcf_suffix(GetParam()), *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

INSTANTIATE_TEST_SUITE_P(All,
                         shadow_pcf_test,
                         testing::Values(pcf_mode::pcf_3x3,
                                         pcf_mode::pcf_5x5,
                                         pcf_mode::pcf_7x7,
                                         pcf_mode::poisson16,
                                         pcf_mode::poisson32));

// =============================================================================
// Toggle wiring — directional_shadows scene with shadows.enabled=false.
// Should match directional_shadows scene minus the shadow contribution
// (i.e. floor under cubes is fully lit, no dark shadow patches).
// =============================================================================
TEST_F(visual_pipeline_test, toggle_shadows_off)
{
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
    auto& cache = renderer.get_cache();

    auto* se = create_lit_shader_effect(AID("se_toggle_sh_off"));
    ASSERT_TRUE(se);

    setup_scene("tsh", glm::vec3(3, 4, 6), glm::vec3(0, 0, 0), se, true);
    // setup_scene enabled shadows; flip the master toggle off.
    renderer.get_render_config().shadows.enabled = false;

    std::vector<texture_sampler_data> no_tex;

    auto* mesh = create_cube_mesh(AID("cube_tsh"), {1, 1, 1});
    auto gpu = make_solid_color_gpu_data(
        {0.6f, 0.1f, 0.1f}, {0.8f, 0.2f, 0.2f}, {1.0f, 1.0f, 1.0f}, 32.0f);
    auto* mat =
        loader.create_material(AID("mat_tsh"), AID("solid_color_material"), no_tex, *se, gpu);
    renderer.schd_add_material(mat);

    auto m1 = glm::translate(glm::mat4(1.0f), glm::vec3(-1.5f, 0.5f, 0.0f));
    auto* o1 = cache.objects.alloc(AID("tsh_cube1"));
    loader.update_object(*o1, *mat, *mesh, m1, glm::transpose(glm::inverse(m1)), {-1.5f, 0.5f, 0});
    renderer.schd_add_object(o1);

    auto m2 = glm::translate(glm::mat4(1.0f), glm::vec3(1.5f, 1.0f, 0.0f)) *
              glm::scale(glm::mat4(1.0f), glm::vec3(0.7f, 2.0f, 0.7f));
    auto* o2 = cache.objects.alloc(AID("tsh_cube2"));
    loader.update_object(*o2, *mat, *mesh, m2, glm::transpose(glm::inverse(m2)), {1.5f, 1.0f, 0});
    renderer.schd_add_object(o2);

    renderer.draw_headless();
    compare("toggle_shadows_off", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// =============================================================================
// Toggle wiring — combined_lights scene with directional contribution disabled.
// Spheres should only show point + spot contributions; left and center spheres
// fall to ambient on faces unlit by point/spot.
// =============================================================================
TEST_F(visual_pipeline_test, toggle_lighting_directional_off)
{
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
    auto& cache = renderer.get_cache();

    auto* se = create_lit_shader_effect(AID("se_tld_off"));
    ASSERT_TRUE(se);

    setup_scene("tld", glm::vec3(0, 4, 8), glm::vec3(0, 0, 0), se);
    renderer.get_render_config().lighting.directional_enabled = false;

    std::vector<texture_sampler_data> no_tex;

    auto* mesh = create_sphere_mesh(AID("sphere_tld"), {1, 1, 1});
    auto gpu = make_solid_color_gpu_data(
        {0.1f, 0.1f, 0.1f}, {0.7f, 0.7f, 0.7f}, {1.0f, 1.0f, 1.0f}, 64.0f);
    auto* mat =
        loader.create_material(AID("mat_tld"), AID("solid_color_material"), no_tex, *se, gpu);
    renderer.schd_add_material(mat);

    float positions[] = {-3.0f, 0.0f, 3.0f};
    const char* names[] = {"tld_l", "tld_c", "tld_r"};
    for (int i = 0; i < 3; ++i)
    {
        auto m = glm::translate(glm::mat4(1.0f), glm::vec3(positions[i], 0, 0));
        auto* obj = cache.objects.alloc(AID(names[i]));
        loader.update_object(
            *obj, *mat, *mesh, m, glm::transpose(glm::inverse(m)), {positions[i], 0, 0});
        renderer.schd_add_object(obj);
    }

    auto* pt = cache.universal_lights.alloc(AID("tld_pt"), light_type::point);
    pt->gpu_data.position = {-3.0f, 1.5f, 2.0f};
    pt->gpu_data.diffuse = {0.0f, 1.0f, 0.0f};
    pt->gpu_data.specular = {0.0f, 1.0f, 0.0f};
    pt->gpu_data.radius = 8.0f;
    pt->gpu_data.type = KGPU_light_type_point;
    pt->gpu_data.cut_off = -1.0f;
    pt->gpu_data.outer_cut_off = -1.0f;
    renderer.schd_add_light(pt);

    auto* sp = cache.universal_lights.alloc(AID("tld_sp"), light_type::spot);
    sp->gpu_data.position = {3.0f, 3.0f, 1.0f};
    sp->gpu_data.direction = glm::normalize(glm::vec3(0, -1.0f, -0.3f));
    sp->gpu_data.diffuse = {1.0f, 0.2f, 0.1f};
    sp->gpu_data.specular = {1.0f, 0.3f, 0.2f};
    sp->gpu_data.radius = 10.0f;
    sp->gpu_data.type = KGPU_light_type_spot;
    sp->gpu_data.cut_off = std::cos(glm::radians(20.0f));
    sp->gpu_data.outer_cut_off = std::cos(glm::radians(30.0f));
    renderer.schd_add_light(sp);

    renderer.draw_headless();
    compare("toggle_lighting_directional_off", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// =============================================================================
// Toggle wiring — combined_lights scene with point+spot disabled.
// Spheres should show only directional contribution; green/red light pools gone.
// =============================================================================
TEST_F(visual_pipeline_test, toggle_lighting_local_off)
{
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
    auto& cache = renderer.get_cache();

    auto* se = create_lit_shader_effect(AID("se_tll_off"));
    ASSERT_TRUE(se);

    setup_scene("tll", glm::vec3(0, 4, 8), glm::vec3(0, 0, 0), se);
    renderer.get_render_config().lighting.local_enabled = false;

    std::vector<texture_sampler_data> no_tex;

    auto* mesh = create_sphere_mesh(AID("sphere_tll"), {1, 1, 1});
    auto gpu = make_solid_color_gpu_data(
        {0.1f, 0.1f, 0.1f}, {0.7f, 0.7f, 0.7f}, {1.0f, 1.0f, 1.0f}, 64.0f);
    auto* mat =
        loader.create_material(AID("mat_tll"), AID("solid_color_material"), no_tex, *se, gpu);
    renderer.schd_add_material(mat);

    float positions[] = {-3.0f, 0.0f, 3.0f};
    const char* names[] = {"tll_l", "tll_c", "tll_r"};
    for (int i = 0; i < 3; ++i)
    {
        auto m = glm::translate(glm::mat4(1.0f), glm::vec3(positions[i], 0, 0));
        auto* obj = cache.objects.alloc(AID(names[i]));
        loader.update_object(
            *obj, *mat, *mesh, m, glm::transpose(glm::inverse(m)), {positions[i], 0, 0});
        renderer.schd_add_object(obj);
    }

    // Same point + spot as combined_lights — should NOT contribute (toggle off).
    auto* pt = cache.universal_lights.alloc(AID("tll_pt"), light_type::point);
    pt->gpu_data.position = {-3.0f, 1.5f, 2.0f};
    pt->gpu_data.diffuse = {0.0f, 1.0f, 0.0f};
    pt->gpu_data.specular = {0.0f, 1.0f, 0.0f};
    pt->gpu_data.radius = 8.0f;
    pt->gpu_data.type = KGPU_light_type_point;
    pt->gpu_data.cut_off = -1.0f;
    pt->gpu_data.outer_cut_off = -1.0f;
    renderer.schd_add_light(pt);

    auto* sp = cache.universal_lights.alloc(AID("tll_sp"), light_type::spot);
    sp->gpu_data.position = {3.0f, 3.0f, 1.0f};
    sp->gpu_data.direction = glm::normalize(glm::vec3(0, -1.0f, -0.3f));
    sp->gpu_data.diffuse = {1.0f, 0.2f, 0.1f};
    sp->gpu_data.specular = {1.0f, 0.3f, 0.2f};
    sp->gpu_data.radius = 10.0f;
    sp->gpu_data.type = KGPU_light_type_spot;
    sp->gpu_data.cut_off = std::cos(glm::radians(20.0f));
    sp->gpu_data.outer_cut_off = std::cos(glm::radians(30.0f));
    renderer.schd_add_light(sp);

    renderer.draw_headless();
    compare("toggle_lighting_local_off", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// =============================================================================
// Cluster lighting stress — 32 colored point lights on a flat floor.
// Stresses cluster cull compute and max_lights_per_cluster (default 32).
// =============================================================================
TEST_F(visual_pipeline_test, culling_cluster_many_lights)
{
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
    auto& cache = renderer.get_cache();

    auto* se = create_lit_shader_effect(AID("se_cluster_many"));
    ASSERT_TRUE(se);

    // Top-down camera, framed tight to the 4x8 light grid.
    gpu::camera_data cam;
    cam.projection =
        glm::perspective(glm::radians(60.0f), float(TEST_WIDTH) / float(TEST_HEIGHT), 0.1f, 256.0f);
    cam.projection[1][1] *= -1.0f;
    cam.view = glm::lookAt(glm::vec3(0, 14, 0.01f), glm::vec3(0, 0, 0), glm::vec3(0, 0, -1));
    cam.inv_projection = glm::inverse(cam.projection);
    cam.position = {0, 14, 0.01f};
    renderer.set_camera(cam);

    // Disabled-contribution sun (renderer needs a directional light buffer slot,
    // but lighting.directional_enabled gates its contribution).
    auto* sun = cache.directional_lights.alloc(AID("clm_null_sun"));
    sun->gpu_data.direction = {0, -1, 0};
    sun->gpu_data.ambient = {0, 0, 0};
    sun->gpu_data.diffuse = {0, 0, 0};
    sun->gpu_data.specular = {0, 0, 0};
    renderer.schd_add_light(sun);
    renderer.set_selected_directional_light(AID("clm_null_sun"));
    renderer.get_render_config().lighting.directional_enabled = false;
    renderer.get_render_config().shadows.distance = 0.0f;

    std::vector<texture_sampler_data> no_tex;

    // Large floor so the light grid fits with margin.
    auto* floor_mesh = create_plane_mesh(AID("clm_floor"), {0.6f, 0.6f, 0.6f}, 24.0f);
    auto floor_gpu =
        make_solid_color_gpu_data({0, 0, 0}, {0.7f, 0.7f, 0.7f}, {0.2f, 0.2f, 0.2f}, 8.0f);
    auto* floor_mat = loader.create_material(
        AID("clm_floor_mat"), AID("solid_color_material"), no_tex, *se, floor_gpu);
    renderer.schd_add_material(floor_mat);

    auto floor_m = glm::translate(glm::mat4(1.0f), glm::vec3(0, -1, 0));
    auto* floor_obj = cache.objects.alloc(AID("clm_floor_obj"));
    loader.update_object(*floor_obj,
                         *floor_mat,
                         *floor_mesh,
                         floor_m,
                         glm::transpose(glm::inverse(floor_m)),
                         {0, -1, 0});
    renderer.schd_add_object(floor_obj);

    // 32 point lights on a 4x8 grid, spaced 2.0 units, just above the floor.
    // Light at y=-0.5 puts the floor (y=-1) within the lit hemisphere with a
    // small distance (~0.5) so quadratic falloff doesn't kill brightness.
    constexpr int rows = 4;
    constexpr int cols = 8;
    constexpr int total = rows * cols;
    for (int i = 0; i < total; ++i)
    {
        int r = i / cols;
        int c = i % cols;
        float x = (c - 3.5f) * 2.0f;
        float z = (r - 1.5f) * 2.0f;

        float hue = float(i) / float(total);
        glm::vec3 color = {
            0.5f + 0.5f * std::cos(6.2832f * hue),
            0.5f + 0.5f * std::cos(6.2832f * (hue + 0.33f)),
            0.5f + 0.5f * std::cos(6.2832f * (hue + 0.66f)),
        };

        auto* pl = cache.universal_lights.alloc(AID(("clm_pl_" + std::to_string(i)).c_str()),
                                                light_type::point);
        pl->gpu_data.position = {x, -0.5f, z};
        pl->gpu_data.ambient = {0, 0, 0};
        pl->gpu_data.diffuse = color;
        pl->gpu_data.specular = color;
        pl->gpu_data.radius = 2.0f;
        pl->gpu_data.type = KGPU_light_type_point;
        pl->gpu_data.cut_off = -1.0f;
        pl->gpu_data.outer_cut_off = -1.0f;
        renderer.schd_add_light(pl);
    }

    renderer.draw_headless();
    compare("culling_cluster_many_lights", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// =============================================================================
// Orthographic projection — three cubes at different depths must render same size.
// =============================================================================
TEST_F(visual_pipeline_test, camera_orthographic)
{
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
    auto& cache = renderer.get_cache();

    auto* se = create_lit_shader_effect(AID("se_ortho"));
    ASSERT_TRUE(se);

    // Orthographic projection — cubes at z=0, -3, -6 must be identical screen size.
    gpu::camera_data cam;
    float ortho_h = 3.0f;
    float ortho_w = ortho_h * float(TEST_WIDTH) / float(TEST_HEIGHT);
    cam.projection = glm::ortho(-ortho_w, ortho_w, -ortho_h, ortho_h, 0.1f, 50.0f);
    cam.projection[1][1] *= -1.0f;
    cam.view = glm::lookAt(glm::vec3(0, 1, 8), glm::vec3(0, 0, -3), glm::vec3(0, 1, 0));
    cam.inv_projection = glm::inverse(cam.projection);
    cam.position = {0, 1, 8};
    renderer.set_camera(cam);

    auto* sun = cache.directional_lights.alloc(AID("ortho_sun"));
    sun->gpu_data.direction = {-0.3f, -1.0f, -0.4f};
    sun->gpu_data.ambient = {0.2f, 0.2f, 0.2f};
    sun->gpu_data.diffuse = {0.9f, 0.9f, 0.9f};
    sun->gpu_data.specular = {0.5f, 0.5f, 0.5f};
    renderer.schd_add_light(sun);
    renderer.set_selected_directional_light(AID("ortho_sun"));
    renderer.get_render_config().shadows.distance = 0.0f;

    std::vector<texture_sampler_data> no_tex;

    auto* mesh = create_cube_mesh(AID("ortho_cube"), {1, 1, 1});
    auto gpu = make_solid_color_gpu_data(
        {0.2f, 0.2f, 0.4f}, {0.4f, 0.4f, 0.8f}, {1.0f, 1.0f, 1.0f}, 32.0f);
    auto* mat =
        loader.create_material(AID("ortho_mat"), AID("solid_color_material"), no_tex, *se, gpu);
    renderer.schd_add_material(mat);

    // Three cubes lined up at increasing depth on the X axis (slightly offset
    // so they don't completely overlap in screen space).
    float positions[][3] = {{-2.0f, 0, 0}, {0.0f, 0, -3}, {2.0f, 0, -6}};
    const char* names[] = {"ortho_a", "ortho_b", "ortho_c"};
    for (int i = 0; i < 3; ++i)
    {
        auto m = glm::translate(glm::mat4(1.0f),
                                glm::vec3(positions[i][0], positions[i][1], positions[i][2]));
        auto* obj = cache.objects.alloc(AID(names[i]));
        loader.update_object(*obj,
                             *mat,
                             *mesh,
                             m,
                             glm::transpose(glm::inverse(m)),
                             {positions[i][0], positions[i][1], positions[i][2]});
        renderer.schd_add_object(obj);
    }

    renderer.draw_headless();
    compare("camera_orthographic", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// =============================================================================
// Frustum culling — ring of 8 cubes, camera framed so half are off-frustum.
// Visible cubes must render correctly (catches: culling kills visible cubes,
// or fails to cull and overdraws).
// =============================================================================
TEST_F(visual_pipeline_test, culling_frustum_basic)
{
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
    auto& cache = renderer.get_cache();

    auto* se = create_lit_shader_effect(AID("se_frustum"));
    ASSERT_TRUE(se);

    // Camera at +z looking toward -z; cubes behind/beside camera will be culled.
    setup_scene("frcl", glm::vec3(0, 1.5f, 6), glm::vec3(0, 0, -3), se);

    std::vector<texture_sampler_data> no_tex;

    auto* mesh = create_cube_mesh(AID("cube_frcl"), {1, 1, 1});
    auto gpu = make_solid_color_gpu_data(
        {0.2f, 0.2f, 0.4f}, {0.4f, 0.4f, 0.8f}, {1.0f, 1.0f, 1.0f}, 32.0f);
    auto* mat =
        loader.create_material(AID("mat_frcl"), AID("solid_color_material"), no_tex, *se, gpu);
    renderer.schd_add_material(mat);

    // 8 cubes in a ring of radius 4 around origin in the XZ plane.
    // Camera at (0, 1.5, 6) looking at (0, 0, -3): cubes at +z (behind camera)
    // and far +x/-x are out of frustum; the front-facing arc is visible.
    constexpr int N = 8;
    for (int i = 0; i < N; ++i)
    {
        float a = (2.0f * 3.14159265f) * float(i) / float(N);
        float x = 4.0f * std::cos(a);
        float z = 4.0f * std::sin(a);

        auto m = glm::translate(glm::mat4(1.0f), glm::vec3(x, 0, z));
        auto* obj = cache.objects.alloc(AID(("frcl_" + std::to_string(i)).c_str()));
        loader.update_object(*obj, *mat, *mesh, m, glm::transpose(glm::inverse(m)), {x, 0, z});
        renderer.schd_add_object(obj);
    }

    renderer.draw_headless();
    compare("culling_frustum_basic", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// =============================================================================
// Render scale — parameterized over divisor 1, 2, 3.
// divisor=1 enabled is distinct from disabled (extra composite pass);
// divisor=2,3 produce nearest-neighbor upscale artifacts.
// =============================================================================
class render_scale_test : public visual_pipeline_test, public testing::WithParamInterface<uint32_t>
{
public:
    static void
    SetUpTestSuite()
    {
        visual_pipeline_test::SetUpTestSuite();
    }
    static void
    TearDownTestSuite()
    {
        visual_pipeline_test::TearDownTestSuite();
    }
};

TEST_P(render_scale_test, looks_correct)
{
    render_config cfg{};
    cfg.debug.show_grid = false;
    cfg.debug.light_wireframe = false;
    cfg.debug.light_icons = false;
    cfg.render_scale.enabled = true;
    cfg.render_scale.divisor = GetParam();
    reinit_with_config(cfg);

    auto& renderer = glob::glob_state().getr_render().renderer;

    // Read back from whichever pass the renderer reports as the host (writes
    // the final swapchain image). With render_scale this is "composite";
    // without it's "main".
    auto* host_pass =
        glob::glob_state().getr_render().loader.get_render_pass(renderer.get_host_pass_id());
    ASSERT_TRUE(host_pass) << "Host pass not found";
    auto& loader = glob::glob_state().getr_render().loader;
    auto& cache = renderer.get_cache();

    auto* se = create_lit_shader_effect(AID("se_rscale"));
    ASSERT_TRUE(se);

    setup_scene("rscale", glm::vec3(2, 2, 5), glm::vec3(0, 0, 0), se);

    std::vector<texture_sampler_data> no_tex;

    // Two cubes for a recognizable scene with edges that show downsampling.
    auto* mesh = create_cube_mesh(AID("cube_rscale"), {1, 1, 1});
    auto red_gpu = make_solid_color_gpu_data(
        {0.6f, 0.1f, 0.1f}, {0.8f, 0.2f, 0.2f}, {1.0f, 1.0f, 1.0f}, 32.0f);
    auto* red_mat = loader.create_material(
        AID("mat_rscale_red"), AID("solid_color_material"), no_tex, *se, red_gpu);
    renderer.schd_add_material(red_mat);

    auto blue_gpu = make_solid_color_gpu_data(
        {0.1f, 0.1f, 0.6f}, {0.2f, 0.2f, 0.8f}, {1.0f, 1.0f, 1.0f}, 32.0f);
    auto* blue_mat = loader.create_material(
        AID("mat_rscale_blue"), AID("solid_color_material"), no_tex, *se, blue_gpu);
    renderer.schd_add_material(blue_mat);

    auto m1 = glm::translate(glm::mat4(1.0f), glm::vec3(-1.0f, 0, 0));
    auto* o1 = cache.objects.alloc(AID("rscale_red"));
    loader.update_object(*o1, *red_mat, *mesh, m1, glm::transpose(glm::inverse(m1)), {-1, 0, 0});
    renderer.schd_add_object(o1);

    auto m2 = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0, 0));
    auto* o2 = cache.objects.alloc(AID("rscale_blue"));
    loader.update_object(*o2, *blue_mat, *mesh, m2, glm::transpose(glm::inverse(m2)), {1, 0, 0});
    renderer.schd_add_object(o2);

    renderer.draw_headless();
    compare("render_scale_div_" + std::to_string(GetParam()), *host_pass, TEST_WIDTH, TEST_HEIGHT);
}

INSTANTIATE_TEST_SUITE_P(All,
                         render_scale_test,
                         testing::Values(uint32_t(1), uint32_t(2), uint32_t(3)));

// =============================================================================
// Grid toggle — init with debug.show_grid=true, render scene with floor + cubes.
// Catches: grid disappears, grid bleeds through opaque geometry, grid color
// regression.
// =============================================================================
TEST_F(visual_pipeline_test, toggle_grid_on)
{
    render_config cfg{};
    cfg.debug.show_grid = true;
    cfg.debug.light_wireframe = false;
    cfg.debug.light_icons = false;
    reinit_with_config(cfg);

    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
    auto& cache = renderer.get_cache();

    auto* se = create_lit_shader_effect(AID("se_grid_on"));
    ASSERT_TRUE(se);

    // Camera angled to see grid stretching to horizon + a single cube for
    // depth occlusion verification.
    gpu::camera_data cam;
    cam.projection =
        glm::perspective(glm::radians(60.0f), float(TEST_WIDTH) / float(TEST_HEIGHT), 0.1f, 256.0f);
    cam.projection[1][1] *= -1.0f;
    cam.view = glm::lookAt(glm::vec3(0, 1.5f, 4), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    cam.inv_projection = glm::inverse(cam.projection);
    cam.position = {0, 1.5f, 4};
    renderer.set_camera(cam);

    auto* sun = cache.directional_lights.alloc(AID("grid_sun"));
    sun->gpu_data.direction = {-0.3f, -1.0f, -0.2f};
    sun->gpu_data.ambient = {0.2f, 0.2f, 0.2f};
    sun->gpu_data.diffuse = {0.9f, 0.9f, 0.9f};
    sun->gpu_data.specular = {0.5f, 0.5f, 0.5f};
    renderer.schd_add_light(sun);
    renderer.set_selected_directional_light(AID("grid_sun"));
    renderer.get_render_config().shadows.distance = 0.0f;

    std::vector<texture_sampler_data> no_tex;

    // One cube sitting on the grid (y=0) — grid lines must be occluded by it.
    auto* mesh = create_cube_mesh(AID("grid_cube"), {1, 1, 1});
    auto gpu = make_solid_color_gpu_data(
        {0.1f, 0.4f, 0.1f}, {0.2f, 0.7f, 0.2f}, {0.5f, 1.0f, 0.5f}, 32.0f);
    auto* mat =
        loader.create_material(AID("grid_mat"), AID("solid_color_material"), no_tex, *se, gpu);
    renderer.schd_add_material(mat);

    auto m = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 0));
    auto* obj = cache.objects.alloc(AID("grid_cube_obj"));
    loader.update_object(*obj, *mat, *mesh, m, glm::transpose(glm::inverse(m)), {0, 0, 0});
    renderer.schd_add_object(obj);

    renderer.draw_headless();
    compare("toggle_grid_on", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// =============================================================================
// Cascade count — parameterized over 1..4.
// Same long-ground scene as shadow_csm_cascades. Validates resource allocation
// and PSSM split selection at each cascade count.
// =============================================================================
class shadow_cascade_test : public visual_pipeline_test,
                            public testing::WithParamInterface<uint32_t>
{
public:
    static void
    SetUpTestSuite()
    {
        visual_pipeline_test::SetUpTestSuite();
    }
    static void
    TearDownTestSuite()
    {
        visual_pipeline_test::TearDownTestSuite();
    }
};

TEST_P(shadow_cascade_test, looks_correct)
{
    render_config cfg{};
    cfg.debug.show_grid = false;
    cfg.debug.light_wireframe = false;
    cfg.debug.light_icons = false;
    cfg.shadows.cascade_count = GetParam();
    reinit_with_config(cfg);

    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
    auto& cache = renderer.get_cache();

    auto* se = create_lit_shader_effect(AID("se_cascade"));
    ASSERT_TRUE(se);

    gpu::camera_data cam;
    cam.projection =
        glm::perspective(glm::radians(60.0f), float(TEST_WIDTH) / float(TEST_HEIGHT), 0.1f, 256.0f);
    cam.projection[1][1] *= -1.0f;
    cam.view = glm::lookAt(glm::vec3(0, 8, 4), glm::vec3(0, 0, -25), glm::vec3(0, 1, 0));
    cam.inv_projection = glm::inverse(cam.projection);
    cam.position = {0, 8, 4};
    renderer.set_camera(cam);

    auto* sun = cache.directional_lights.alloc(AID("casc_sun"));
    sun->gpu_data.direction = {-0.3f, -1.0f, -0.2f};
    sun->gpu_data.ambient = {0.15f, 0.15f, 0.15f};
    sun->gpu_data.diffuse = {1.0f, 1.0f, 1.0f};
    sun->gpu_data.specular = {0.5f, 0.5f, 0.5f};
    renderer.schd_add_light(sun);
    renderer.set_selected_directional_light(AID("casc_sun"));
    renderer.get_render_config().shadows.distance = 100.0f;

    std::vector<texture_sampler_data> no_tex;

    auto* ground = create_plane_mesh(AID("casc_ground"), {0.6f, 0.6f, 0.6f}, 60.0f);
    auto ground_gpu =
        make_solid_color_gpu_data({0.3f, 0.3f, 0.3f}, {0.6f, 0.6f, 0.6f}, {0.1f, 0.1f, 0.1f}, 8.0f);
    auto* ground_mat = loader.create_material(
        AID("casc_ground_mat"), AID("solid_color_material"), no_tex, *se, ground_gpu);
    renderer.schd_add_material(ground_mat);

    auto ground_m = glm::translate(glm::mat4(1.0f), glm::vec3(0, -1, -25));
    auto* ground_obj = cache.objects.alloc(AID("casc_ground_obj"));
    loader.update_object(*ground_obj,
                         *ground_mat,
                         *ground,
                         ground_m,
                         glm::transpose(glm::inverse(ground_m)),
                         {0, -1, -25});
    renderer.schd_add_object(ground_obj);

    auto* cube_mesh = create_cube_mesh(AID("casc_cube"), {1, 1, 1});
    auto cube_gpu = make_solid_color_gpu_data(
        {0.2f, 0.1f, 0.1f}, {0.7f, 0.2f, 0.2f}, {0.5f, 0.5f, 0.5f}, 32.0f);
    auto* cube_mat = loader.create_material(
        AID("casc_cube_mat"), AID("solid_color_material"), no_tex, *se, cube_gpu);
    renderer.schd_add_material(cube_mat);

    float depths[] = {-2.0f, -10.0f, -25.0f, -45.0f};
    for (int i = 0; i < 4; ++i)
    {
        auto m = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, depths[i]));
        auto* obj = cache.objects.alloc(AID(("casc_cube_" + std::to_string(i)).c_str()));
        loader.update_object(
            *obj, *cube_mat, *cube_mesh, m, glm::transpose(glm::inverse(m)), {0, 0, depths[i]});
        renderer.schd_add_object(obj);
    }

    renderer.draw_headless();
    compare("shadow_cascade_count_" + std::to_string(GetParam()),
            *m_main_pass,
            TEST_WIDTH,
            TEST_HEIGHT);
}

INSTANTIATE_TEST_SUITE_P(All,
                         shadow_cascade_test,
                         testing::Values(uint32_t(1), uint32_t(2), uint32_t(3), uint32_t(4)));

TEST_F(visual_pipeline_test, shader_toon)
{
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
    auto& cache = renderer.get_cache();

    kryga::utils::buffer vert_buf, frag_buf;
    auto se_path = glob::glob_state().getr_vfs().real_path(
        vfs::rid("data://packages/base.apkg/class/shader_effects/lit"));
    auto path = APATH(se_path.value());
    kryga::utils::buffer::load(path / "se_lit.vert.spv", vert_buf);
    kryga::utils::buffer::load(path / "se_toon.frag.spv", frag_buf);

    shader_effect_create_info se_ci;
    se_ci.vert_buffer = &vert_buf;
    se_ci.frag_buffer = &frag_buf;
    se_ci.cull_mode = VK_CULL_MODE_BACK_BIT;
    se_ci.width = TEST_WIDTH;
    se_ci.height = TEST_HEIGHT;

    shader_effect_data* se = nullptr;
    m_main_pass->create_shader_effect(AID("se_toon_test"), se_ci, se);
    ASSERT_TRUE(se);

    // Skip setup_scene — floor's solid_color material interferes with the
    // sphere's bindless texture sampling under toon (engine batching bug).
    gpu::camera_data cam;
    cam.projection =
        glm::perspective(glm::radians(50.0f), float(TEST_WIDTH) / float(TEST_HEIGHT), 0.1f, 256.0f);
    cam.projection[1][1] *= -1.0f;
    cam.view = glm::lookAt(glm::vec3(0, 0, 2), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    cam.inv_projection = glm::inverse(cam.projection);
    cam.position = {0, 0, 2};
    renderer.set_camera(cam);

    auto* sun = cache.directional_lights.alloc(AID("toon_sun"));
    sun->gpu_data.direction = {-0.5f, -0.7f, -0.3f};
    sun->gpu_data.ambient = {0.2f, 0.2f, 0.2f};
    sun->gpu_data.diffuse = {0.9f, 0.9f, 0.9f};
    sun->gpu_data.specular = {1.0f, 1.0f, 1.0f};
    renderer.schd_add_light(sun);
    renderer.set_selected_directional_light(AID("toon_sun"));
    renderer.get_render_config().shadows.distance = 0.0f;

    auto* tex = create_checker_texture(AID("toon_checker"));
    ASSERT_TRUE(tex);
    uint32_t tex_idx = tex->get_bindless_index();

    auto* mesh = create_sphere_mesh(AID("toon_sphere"), {1, 1, 1}, 32, 32);

    gpu::toon_material__gpu mat_gpu{};
    for (int i = 0; i < KGPU_MAX_TEXTURE_SLOTS; ++i)
    {
        mat_gpu.texture_indices[i] = UINT32_MAX;
        mat_gpu.sampler_indices[i] = UINT32_MAX;
    }
    mat_gpu.texture_indices[KGPU_TEXTURE_SLOT_ALBEDO] = tex_idx;
    mat_gpu.sampler_indices[KGPU_TEXTURE_SLOT_ALBEDO] = KGPU_SAMPLER_LINEAR_REPEAT;
    mat_gpu.band_count = 4.0f;
    mat_gpu.shininess = 64.0f;
    mat_gpu.specular_strength = 0.9f;

    utils::dynobj toon_data;
    toon_data.resize(sizeof(mat_gpu));
    std::memcpy(toon_data.data(), &mat_gpu, sizeof(mat_gpu));

    std::vector<texture_sampler_data> samples;
    samples.push_back({tex, VK_NULL_HANDLE, KGPU_TEXTURE_SLOT_ALBEDO});
    auto* mat =
        loader.create_material(AID("mat_toon"), AID("toon_material"), samples, *se, toon_data);
    renderer.schd_add_material(mat);

    auto m = glm::mat4(1.0f);
    auto* obj = cache.objects.alloc(AID("toon_obj"));
    loader.update_object(*obj, *mat, *mesh, m, glm::transpose(glm::inverse(m)), {0, 0, 0});
    renderer.schd_add_object(obj);

    for (int i = 0; i < 4; ++i)
    {
        renderer.draw_headless();
    }
    compare("shader_toon", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// =============================================================================
// Material emissive — simulated via very high ambient + dim/no light contrib.
// Left cube: low ambient (looks dark). Right cube: high ambient (glows).
// =============================================================================
TEST_F(visual_pipeline_test, material_emissive)
{
    // No real "emissive" channel in the engine yet — simulate by using the
    // unlit shader for the "hot" cube (renders flat color regardless of light)
    // vs the lit shader for the "cold" cube under dim light. The test catches
    // accidental coupling between unlit shader and ambient/light contribution.
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
    auto& cache = renderer.get_cache();

    auto* lit_se = create_lit_shader_effect(AID("se_emi_lit"));
    auto* unlit_se = create_unlit_shader_effect(AID("se_emi_unlit"));
    ASSERT_TRUE(lit_se);
    ASSERT_TRUE(unlit_se);

    gpu::camera_data cam;
    cam.projection =
        glm::perspective(glm::radians(50.0f), float(TEST_WIDTH) / float(TEST_HEIGHT), 0.1f, 256.0f);
    cam.projection[1][1] *= -1.0f;
    cam.view = glm::lookAt(glm::vec3(0, 1, 5), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    cam.inv_projection = glm::inverse(cam.projection);
    cam.position = {0, 1, 5};
    renderer.set_camera(cam);

    auto* sun = cache.directional_lights.alloc(AID("emi_sun"));
    sun->gpu_data.direction = {-0.3f, -1.0f, -0.2f};
    sun->gpu_data.ambient = {0.05f, 0.05f, 0.05f};
    sun->gpu_data.diffuse = {0.15f, 0.15f, 0.15f};
    sun->gpu_data.specular = {0.0f, 0.0f, 0.0f};
    renderer.schd_add_light(sun);
    renderer.set_selected_directional_light(AID("emi_sun"));
    renderer.get_render_config().shadows.distance = 0.0f;

    std::vector<texture_sampler_data> no_tex;

    auto* mesh = create_cube_mesh(AID("emi_cube"), {1, 1, 1});

    // Left: lit shader, blue diffuse — appears dim under low light.
    auto cold_gpu =
        make_solid_color_gpu_data({0.1f, 0.1f, 0.3f}, {0.2f, 0.2f, 0.6f}, {0, 0, 0}, 1.0f);
    auto* cold_mat = loader.create_material(
        AID("mat_emi_cold"), AID("solid_color_material"), no_tex, *lit_se, cold_gpu);
    renderer.schd_add_material(cold_mat);

    auto m1 = glm::translate(glm::mat4(1.0f), glm::vec3(-1.2f, 0, 0));
    auto* o1 = cache.objects.alloc(AID("emi_cold"));
    loader.update_object(
        *o1, *cold_mat, *mesh, m1, glm::transpose(glm::inverse(m1)), {-1.2f, 0, 0});
    renderer.schd_add_object(o1);

    // Right: unlit shader with bright yellow — renders flat-bright (~emissive).
    auto hot_gpu =
        make_solid_color_gpu_data({1.0f, 0.9f, 0.2f}, {1.0f, 0.9f, 0.2f}, {0, 0, 0}, 1.0f);
    auto* hot_mat = loader.create_material(
        AID("mat_emi_hot"), AID("solid_color_material"), no_tex, *unlit_se, hot_gpu);
    renderer.schd_add_material(hot_mat);

    auto m2 = glm::translate(glm::mat4(1.0f), glm::vec3(1.2f, 0, 0));
    auto* o2 = cache.objects.alloc(AID("emi_hot"));
    loader.update_object(*o2, *hot_mat, *mesh, m2, glm::transpose(glm::inverse(m2)), {1.2f, 0, 0});
    renderer.schd_add_object(o2);

    renderer.draw_headless();
    compare("material_emissive", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// =============================================================================
// Alpha sorting — three transparent quads at different depths, partly overlapped.
// Wrong sort order would invert the visible color stack.
// =============================================================================
TEST_F(visual_pipeline_test, material_alpha_sorting)
{
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
    auto& cache = renderer.get_cache();

    auto* se = create_transparent_shader_effect(AID("se_alpha_sort"));
    ASSERT_TRUE(se);

    gpu::camera_data cam;
    cam.projection =
        glm::perspective(glm::radians(50.0f), float(TEST_WIDTH) / float(TEST_HEIGHT), 0.1f, 256.0f);
    cam.projection[1][1] *= -1.0f;
    cam.view = glm::lookAt(glm::vec3(0, 0, 6), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    cam.inv_projection = glm::inverse(cam.projection);
    cam.position = {0, 0, 6};
    renderer.set_camera(cam);
    renderer.get_render_config().shadows.distance = 0.0f;

    std::vector<texture_sampler_data> no_tex;

    // Three large quads (cubes scaled flat), staggered in xy and depth.
    auto* mesh = create_cube_mesh(AID("alpha_sort_quad"), {1, 1, 1});

    struct entry
    {
        const char* name;
        glm::vec3 pos;
        glm::vec3 color;
        float opacity;
    };
    entry entries[] = {
        {"alpha_sort_red", {-0.6f, 0.3f, 0}, {1, 0.1f, 0.1f}, 0.5f},
        {"alpha_sort_grn", {0, -0.3f, -1}, {0.1f, 1, 0.1f}, 0.5f},
        {"alpha_sort_blu", {0.6f, 0.3f, -2}, {0.1f, 0.1f, 1}, 0.5f},
    };

    for (auto& e : entries)
    {
        auto gpu = make_solid_color_alpha_gpu_data(e.color, e.color, {0, 0, 0}, 1.0f, e.opacity);
        auto* mat = loader.create_material(AID((std::string("mat_") + e.name).c_str()),
                                           AID("solid_color_alpha_material"),
                                           no_tex,
                                           *se,
                                           gpu);
        renderer.schd_add_material(mat);

        auto m = glm::translate(glm::mat4(1.0f), e.pos) *
                 glm::scale(glm::mat4(1.0f), glm::vec3(1.5f, 1.5f, 0.05f));
        auto* obj = cache.objects.alloc(AID(e.name));
        obj->queue_id = "transparent";
        loader.update_object(*obj, *mat, *mesh, m, glm::transpose(glm::inverse(m)), e.pos);
        renderer.schd_add_object(obj);
    }

    renderer.draw_headless();
    compare("material_alpha_sorting", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// =============================================================================
// Point light shadows (DPSM) — corner walls + cube + point light.
// Validates dual-paraboloid shadow path for omnidirectional point lights.
// =============================================================================
TEST_F(visual_pipeline_test, shadow_point_dpsm)
{
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
    auto& cache = renderer.get_cache();

    auto* se = create_lit_shader_effect(AID("se_point_dpsm"));
    ASSERT_TRUE(se);

    gpu::camera_data cam;
    cam.projection =
        glm::perspective(glm::radians(60.0f), float(TEST_WIDTH) / float(TEST_HEIGHT), 0.1f, 256.0f);
    cam.projection[1][1] *= -1.0f;
    cam.view = glm::lookAt(glm::vec3(3, 2.5f, 3.5f), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    cam.inv_projection = glm::inverse(cam.projection);
    cam.position = {3, 2.5f, 3.5f};
    renderer.set_camera(cam);

    // Disable directional contribution — only the point light + its shadows.
    auto* null_sun = cache.directional_lights.alloc(AID("dpsm_null_sun"));
    null_sun->gpu_data.direction = {0, -1, 0};
    null_sun->gpu_data.ambient = {0, 0, 0};
    null_sun->gpu_data.diffuse = {0, 0, 0};
    null_sun->gpu_data.specular = {0, 0, 0};
    renderer.schd_add_light(null_sun);
    renderer.set_selected_directional_light(AID("dpsm_null_sun"));
    renderer.get_render_config().lighting.directional_enabled = false;
    renderer.get_render_config().shadows.distance = 50.0f;
    renderer.get_render_config().shadows.enabled = true;

    std::vector<texture_sampler_data> no_tex;

    // Floor + back wall (-z) + side wall (-x) forming a corner.
    auto wall_gpu =
        make_solid_color_gpu_data({0.0f, 0.0f, 0.0f}, {0.7f, 0.7f, 0.7f}, {0.1f, 0.1f, 0.1f}, 8.0f);
    auto* wall_mat = loader.create_material(
        AID("dpsm_wall_mat"), AID("solid_color_material"), no_tex, *se, wall_gpu);
    renderer.schd_add_material(wall_mat);

    // Floor
    auto* floor = create_plane_mesh(AID("dpsm_floor"), {0.7f, 0.7f, 0.7f}, 8.0f);
    auto fm = glm::translate(glm::mat4(1.0f), glm::vec3(0, -1.5f, 0));
    auto* floor_obj = cache.objects.alloc(AID("dpsm_floor_obj"));
    loader.update_object(
        *floor_obj, *wall_mat, *floor, fm, glm::transpose(glm::inverse(fm)), {0, -1.5f, 0});
    renderer.schd_add_object(floor_obj);

    // Back wall (XY plane at z=-2)
    auto* back = create_cube_mesh(AID("dpsm_back"), {0.7f, 0.7f, 0.7f});
    auto bm = glm::translate(glm::mat4(1.0f), glm::vec3(0, 1, -2.5f)) *
              glm::scale(glm::mat4(1.0f), glm::vec3(8.0f, 5.0f, 0.2f));
    auto* back_obj = cache.objects.alloc(AID("dpsm_back_obj"));
    loader.update_object(
        *back_obj, *wall_mat, *back, bm, glm::transpose(glm::inverse(bm)), {0, 1, -2.5f});
    renderer.schd_add_object(back_obj);

    // Side wall (YZ plane at x=-2.5)
    auto* side = create_cube_mesh(AID("dpsm_side"), {0.7f, 0.7f, 0.7f});
    auto sm = glm::translate(glm::mat4(1.0f), glm::vec3(-2.5f, 1, 0)) *
              glm::scale(glm::mat4(1.0f), glm::vec3(0.2f, 5.0f, 8.0f));
    auto* side_obj = cache.objects.alloc(AID("dpsm_side_obj"));
    loader.update_object(
        *side_obj, *wall_mat, *side, sm, glm::transpose(glm::inverse(sm)), {-2.5f, 1, 0});
    renderer.schd_add_object(side_obj);

    // Occluder cube near the corner
    auto cube_gpu = make_solid_color_gpu_data(
        {0.0f, 0.0f, 0.0f}, {0.5f, 0.2f, 0.2f}, {0.5f, 0.5f, 0.5f}, 32.0f);
    auto* cube_mat = loader.create_material(
        AID("dpsm_cube_mat"), AID("solid_color_material"), no_tex, *se, cube_gpu);
    renderer.schd_add_material(cube_mat);

    auto* occ = create_cube_mesh(AID("dpsm_occ"), {1, 0.4f, 0.4f});
    auto cm = glm::translate(glm::mat4(1.0f), glm::vec3(0, -0.8f, 0));
    auto* occ_obj = cache.objects.alloc(AID("dpsm_occ_obj"));
    loader.update_object(
        *occ_obj, *cube_mat, *occ, cm, glm::transpose(glm::inverse(cm)), {0, -0.8f, 0});
    renderer.schd_add_object(occ_obj);

    // Point light hovering above and forward of the occluder, casting shadows
    // onto floor + back wall + side wall in different directions (DPSM territory).
    auto* pl = cache.universal_lights.alloc(AID("dpsm_pl"), light_type::point);
    pl->gpu_data.position = {1.0f, 1.5f, 1.0f};
    pl->gpu_data.ambient = {0, 0, 0};
    pl->gpu_data.diffuse = {1.0f, 1.0f, 0.9f};
    pl->gpu_data.specular = {1.0f, 1.0f, 0.9f};
    pl->gpu_data.radius = 12.0f;
    pl->gpu_data.type = KGPU_light_type_point;
    pl->gpu_data.cut_off = -1.0f;
    pl->gpu_data.outer_cut_off = -1.0f;
    renderer.schd_add_light(pl);

    renderer.draw_headless();
    compare("shadow_point_dpsm", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// =============================================================================
// Instanced draw — 100 cubes sharing one mesh + material. Renderer auto-batches
// into instanced draw calls. Catches gl_InstanceIndex / instance_slots regressions.
// =============================================================================
TEST_F(visual_pipeline_test, mesh_instanced_draw)
{
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
    auto& cache = renderer.get_cache();

    auto* se = create_lit_shader_effect(AID("se_inst"));
    ASSERT_TRUE(se);

    gpu::camera_data cam;
    cam.projection =
        glm::perspective(glm::radians(45.0f), float(TEST_WIDTH) / float(TEST_HEIGHT), 0.1f, 256.0f);
    cam.projection[1][1] *= -1.0f;
    cam.view = glm::lookAt(glm::vec3(8, 12, 14), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    cam.inv_projection = glm::inverse(cam.projection);
    cam.position = {8, 12, 14};
    renderer.set_camera(cam);

    auto* sun = cache.directional_lights.alloc(AID("inst_sun"));
    sun->gpu_data.direction = {-0.3f, -1.0f, -0.2f};
    sun->gpu_data.ambient = {0.2f, 0.2f, 0.2f};
    sun->gpu_data.diffuse = {0.9f, 0.9f, 0.9f};
    sun->gpu_data.specular = {0.5f, 0.5f, 0.5f};
    renderer.schd_add_light(sun);
    renderer.set_selected_directional_light(AID("inst_sun"));
    renderer.get_render_config().shadows.distance = 0.0f;

    std::vector<texture_sampler_data> no_tex;

    // Single shared mesh + material — must render via instanced draw.
    auto* mesh = create_cube_mesh(AID("inst_cube"), {1, 1, 1});
    auto gpu = make_solid_color_gpu_data(
        {0.1f, 0.2f, 0.4f}, {0.3f, 0.5f, 0.8f}, {0.6f, 0.8f, 1.0f}, 32.0f);
    auto* mat =
        loader.create_material(AID("inst_mat"), AID("solid_color_material"), no_tex, *se, gpu);
    renderer.schd_add_material(mat);

    // 10x10 grid of instances at y=0.
    constexpr int side = 10;
    for (int i = 0; i < side; ++i)
    {
        for (int j = 0; j < side; ++j)
        {
            float x = (float(i) - (side - 1) * 0.5f) * 1.5f;
            float z = (float(j) - (side - 1) * 0.5f) * 1.5f;
            auto m = glm::translate(glm::mat4(1.0f), glm::vec3(x, 0, z));
            auto* obj = cache.objects.alloc(AID(("inst_" + std::to_string(i * side + j)).c_str()));
            loader.update_object(*obj, *mat, *mesh, m, glm::transpose(glm::inverse(m)), {x, 0, z});
            renderer.schd_add_object(obj);
        }
    }

    renderer.draw_headless();
    compare("mesh_instanced_draw", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// =============================================================================
// Grid toggle off — same scene as toggle_grid_on but grid disabled.
// Diff against toggle_grid_on isolates the grid contribution.
// =============================================================================
TEST_F(visual_pipeline_test, toggle_grid_off)
{
    // Default SetUp() already inits with debug.show_grid=false — no reinit needed.
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
    auto& cache = renderer.get_cache();

    auto* se = create_lit_shader_effect(AID("se_grid_off"));
    ASSERT_TRUE(se);

    // Same camera + cube as toggle_grid_on.
    gpu::camera_data cam;
    cam.projection =
        glm::perspective(glm::radians(60.0f), float(TEST_WIDTH) / float(TEST_HEIGHT), 0.1f, 256.0f);
    cam.projection[1][1] *= -1.0f;
    cam.view = glm::lookAt(glm::vec3(0, 1.5f, 4), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    cam.inv_projection = glm::inverse(cam.projection);
    cam.position = {0, 1.5f, 4};
    renderer.set_camera(cam);

    auto* sun = cache.directional_lights.alloc(AID("grid_off_sun"));
    sun->gpu_data.direction = {-0.3f, -1.0f, -0.2f};
    sun->gpu_data.ambient = {0.2f, 0.2f, 0.2f};
    sun->gpu_data.diffuse = {0.9f, 0.9f, 0.9f};
    sun->gpu_data.specular = {0.5f, 0.5f, 0.5f};
    renderer.schd_add_light(sun);
    renderer.set_selected_directional_light(AID("grid_off_sun"));
    renderer.get_render_config().shadows.distance = 0.0f;

    std::vector<texture_sampler_data> no_tex;

    auto* mesh = create_cube_mesh(AID("grid_off_cube"), {1, 1, 1});
    auto gpu = make_solid_color_gpu_data(
        {0.1f, 0.4f, 0.1f}, {0.2f, 0.7f, 0.2f}, {0.5f, 1.0f, 0.5f}, 32.0f);
    auto* mat =
        loader.create_material(AID("grid_off_mat"), AID("solid_color_material"), no_tex, *se, gpu);
    renderer.schd_add_material(mat);

    auto m = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 0));
    auto* obj = cache.objects.alloc(AID("grid_off_obj"));
    loader.update_object(*obj, *mat, *mesh, m, glm::transpose(glm::inverse(m)), {0, 0, 0});
    renderer.schd_add_object(obj);

    renderer.draw_headless();
    compare("toggle_grid_off", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// =============================================================================
// Skipped tests — placeholders for plan items blocked by external assets,
// engine bugs, or pending investigation. Each documents what unblocks it.
// =============================================================================

// Procedural 2-bone skinned cuboid. Bottom verts (y<0) ride bone 0 (identity);
// top verts (y>0) ride bone 1, which rotates -45° around X so the upper half
// bends forward. Verifies the skinned vertex format, bone-matrix SSBO upload,
// and per-instance bone_offset/bone_count plumbing on object_data.
TEST_F(visual_pipeline_test, mesh_skinned_basic)
{
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
    auto& cache = renderer.get_cache();

    // Skinned vert + standard solid-color lit frag (frag never reads
    // in_lightmap_uv when ENABLE_LIGHTMAP=false, so the skinned vert leaving
    // out_lightmap_uv unwritten is safe).
    kryga::utils::buffer vert_buf, frag_buf;
    auto path_rp = glob::glob_state().getr_vfs().real_path(
        vfs::rid("data://packages/base.apkg/class/shader_effects/lit"));
    auto path = APATH(path_rp.value());
    kryga::utils::buffer::load(path / "se_simple_texture_lit_skinned.vert.spv", vert_buf);
    kryga::utils::buffer::load(path / "se_solid_color_lit.frag.spv", frag_buf);

    shader_effect_create_info se_ci;
    se_ci.vert_buffer = &vert_buf;
    se_ci.frag_buffer = &frag_buf;
    se_ci.cull_mode = VK_CULL_MODE_BACK_BIT;
    se_ci.width = TEST_WIDTH;
    se_ci.height = TEST_HEIGHT;

    shader_effect_data* se = nullptr;
    auto se_rc = m_main_pass->create_shader_effect(AID("se_skinned"), se_ci, se);
    ASSERT_EQ(se_rc, result_code::ok);
    ASSERT_TRUE(se);
    ASSERT_FALSE(se->m_failed_load);

    // Build a tall thin cuboid (0.5 x 2 x 0.5). Bottom 4 verts (y<0)
    // weighted to bone 0; top 4 verts (y>0) weighted to bone 1.
    auto bw0 = []() { return glm::uvec4{0, 0, 0, 0}; };
    auto bw1 = []() { return glm::uvec4{1, 0, 0, 0}; };
    glm::vec4 w_full = {1.0f, 0.0f, 0.0f, 0.0f};

    auto mk = [&](glm::vec3 p, glm::vec3 n, glm::vec3 c, glm::vec2 uv, glm::uvec4 bi, glm::vec4 bw)
    { return gpu::skinned_vertex_data{p, n, c, uv, bi, bw}; };

    // 24 verts (6 faces * 4) so each face has its own normals.
    std::vector<gpu::skinned_vertex_data> verts;
    glm::vec3 col = {0.85f, 0.85f, 0.85f};
    auto add_face = [&](glm::vec3 a, glm::vec3 b, glm::vec3 cc, glm::vec3 d, glm::vec3 n)
    {
        auto bi_a = (a.y > 0) ? bw1() : bw0();
        auto bi_b = (b.y > 0) ? bw1() : bw0();
        auto bi_c = (cc.y > 0) ? bw1() : bw0();
        auto bi_d = (d.y > 0) ? bw1() : bw0();
        verts.push_back(mk(a, n, col, {0, 0}, bi_a, w_full));
        verts.push_back(mk(b, n, col, {1, 0}, bi_b, w_full));
        verts.push_back(mk(cc, n, col, {1, 1}, bi_c, w_full));
        verts.push_back(mk(d, n, col, {0, 1}, bi_d, w_full));
    };

    constexpr float hx = 0.25f, hy = 1.0f, hz = 0.25f;
    // Front (+Z)
    add_face({-hx, -hy, hz}, {hx, -hy, hz}, {hx, hy, hz}, {-hx, hy, hz}, {0, 0, 1});
    // Back (-Z)
    add_face({hx, -hy, -hz}, {-hx, -hy, -hz}, {-hx, hy, -hz}, {hx, hy, -hz}, {0, 0, -1});
    // Top (+Y)  — all top, all bone 1
    add_face({-hx, hy, hz}, {hx, hy, hz}, {hx, hy, -hz}, {-hx, hy, -hz}, {0, 1, 0});
    // Bottom (-Y) — all bottom, all bone 0
    add_face({-hx, -hy, -hz}, {hx, -hy, -hz}, {hx, -hy, hz}, {-hx, -hy, hz}, {0, -1, 0});
    // Right (+X)
    add_face({hx, -hy, hz}, {hx, -hy, -hz}, {hx, hy, -hz}, {hx, hy, hz}, {1, 0, 0});
    // Left (-X)
    add_face({-hx, -hy, -hz}, {-hx, -hy, hz}, {-hx, hy, hz}, {-hx, hy, -hz}, {-1, 0, 0});

    std::vector<gpu::uint> indices;
    for (uint32_t f = 0; f < 6; ++f)
    {
        uint32_t b = f * 4;
        indices.insert(indices.end(), {b, b + 1, b + 2, b + 2, b + 3, b});
    }

    kryga::utils::buffer vb(verts.size() * sizeof(gpu::skinned_vertex_data));
    std::memcpy(vb.data(), verts.data(), vb.size());
    kryga::utils::buffer ib(indices.size() * sizeof(gpu::uint));
    std::memcpy(ib.data(), indices.data(), ib.size());

    auto* mesh = loader.create_skinned_mesh(
        AID("skinned_arm"), vb.make_view<gpu::skinned_vertex_data>(), ib.make_view<gpu::uint>());
    ASSERT_TRUE(mesh);

    // Material — tan/grey.
    auto mat_gpu = make_solid_color_gpu_data(
        {0.2f, 0.2f, 0.2f}, {0.7f, 0.55f, 0.4f}, {0.4f, 0.4f, 0.4f}, 24.0f);
    std::vector<texture_sampler_data> no_tex;
    auto* mat = loader.create_material(
        AID("skinned_mat"), AID("solid_color_material"), no_tex, *se, mat_gpu);
    renderer.schd_add_material(mat);

    // Push two bone matrices into staging:
    //   bone 0 = identity
    //   bone 1 = rotation -45° around X (top half tilts toward camera)
    auto& bone_staging = renderer.get_bone_matrices_staging();
    const uint32_t bone_offset = static_cast<uint32_t>(bone_staging.size());
    bone_staging.push_back(glm::mat4(1.0f));
    bone_staging.push_back(glm::rotate(glm::mat4(1.0f), glm::radians(-45.0f), glm::vec3(1, 0, 0)));

    // Camera + sun.
    gpu::camera_data cam;
    cam.projection =
        glm::perspective(glm::radians(45.0f), float(TEST_WIDTH) / float(TEST_HEIGHT), 0.1f, 100.0f);
    cam.projection[1][1] *= -1.0f;
    cam.view =
        glm::lookAt(glm::vec3(2.0f, 1.5f, 4.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0, 1, 0));
    cam.inv_projection = glm::inverse(cam.projection);
    cam.position = {2.0f, 1.5f, 4.0f};
    renderer.set_camera(cam);

    auto* sun = cache.directional_lights.alloc(AID("skin_sun"));
    sun->gpu_data.direction = {-0.3f, -1.0f, -0.4f};
    sun->gpu_data.ambient = {0.25f, 0.25f, 0.25f};
    sun->gpu_data.diffuse = {0.9f, 0.9f, 0.9f};
    sun->gpu_data.specular = {0.5f, 0.5f, 0.5f};
    renderer.schd_add_light(sun);
    renderer.set_selected_directional_light(AID("skin_sun"));
    renderer.get_render_config().shadows.distance = 0.0f;

    // Object — bind bone palette (offset+count) so the vertex shader knows it
    // is skinned. update_object zeroes bone fields, so set them after.
    auto* obj = cache.objects.alloc(AID("skin_obj"));
    auto model = glm::mat4(1.0f);
    loader.update_object(*obj, *mat, *mesh, model, glm::transpose(glm::inverse(model)), {0, 0, 0});
    obj->gpu_data.bone_offset = bone_offset;
    obj->gpu_data.bone_count = 2;
    renderer.schd_add_object(obj);

    renderer.draw_headless();
    compare("mesh_skinned_basic", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

TEST_F(visual_pipeline_test, material_textured_albedo)
{
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
    auto& cache = renderer.get_cache();

    auto* se = create_pbr_lit_shader_effect(AID("se_tex_albedo"));
    ASSERT_TRUE(se);

    // Skip setup_scene — render only the textured cube to isolate the
    // lit-shader bindless sampling path from any floor-material interference.
    gpu::camera_data cam;
    cam.projection =
        glm::perspective(glm::radians(50.0f), float(TEST_WIDTH) / float(TEST_HEIGHT), 0.1f, 256.0f);
    cam.projection[1][1] *= -1.0f;
    cam.view = glm::lookAt(glm::vec3(0, 0, 3), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    cam.inv_projection = glm::inverse(cam.projection);
    cam.position = {0, 0, 3};
    renderer.set_camera(cam);

    auto* sun = cache.directional_lights.alloc(AID("texa_sun"));
    sun->gpu_data.direction = {-0.3f, -1.0f, -0.2f};
    sun->gpu_data.ambient = {0.4f, 0.4f, 0.4f};
    sun->gpu_data.diffuse = {0.8f, 0.8f, 0.8f};
    sun->gpu_data.specular = {0, 0, 0};
    renderer.schd_add_light(sun);
    renderer.set_selected_directional_light(AID("texa_sun"));
    renderer.get_render_config().shadows.distance = 0.0f;

    auto* tex = create_checker_texture(AID("tex_albedo_checker"));
    ASSERT_TRUE(tex);
    uint32_t tex_idx = tex->get_bindless_index();

    auto* mesh = create_cube_mesh(AID("tex_albedo_cube"), {1, 1, 1});

    gpu::pbr_material__gpu mat_gpu{};
    for (int i = 0; i < KGPU_MAX_TEXTURE_SLOTS; ++i)
    {
        mat_gpu.texture_indices[i] = UINT32_MAX;
        mat_gpu.sampler_indices[i] = UINT32_MAX;
    }
    mat_gpu.texture_indices[KGPU_TEXTURE_SLOT_ALBEDO] = tex_idx;
    mat_gpu.sampler_indices[KGPU_TEXTURE_SLOT_ALBEDO] = KGPU_SAMPLER_LINEAR_REPEAT;
    mat_gpu.ambient = {0.2f, 0.2f, 0.2f};
    mat_gpu.diffuse = {1, 1, 1};
    mat_gpu.specular = {0, 0, 0};
    mat_gpu.shininess = 32.0f;

    utils::dynobj data;
    data.resize(sizeof(mat_gpu));
    std::memcpy(data.data(), &mat_gpu, sizeof(mat_gpu));

    std::vector<texture_sampler_data> samples;
    samples.push_back({tex, VK_NULL_HANDLE, KGPU_TEXTURE_SLOT_ALBEDO});
    auto* mat =
        loader.create_material(AID("mat_tex_albedo"), AID("pbr_material"), samples, *se, data);
    renderer.schd_add_material(mat);

    auto m = glm::rotate(glm::mat4(1.0f), glm::radians(25.0f), glm::vec3(0, 1, 0));
    auto* obj = cache.objects.alloc(AID("tex_albedo_obj"));
    loader.update_object(*obj, *mat, *mesh, m, glm::transpose(glm::inverse(m)), {0, 0, 0});
    renderer.schd_add_object(obj);

    for (int i = 0; i < 4; ++i)
    {
        renderer.draw_headless();
    }
    compare("material_textured_albedo", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

TEST_F(visual_pipeline_test, material_pbr_textured)
{
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
    auto& cache = renderer.get_cache();

    auto* se = create_pbr_lit_shader_effect(AID("se_pbr_tex"));
    ASSERT_TRUE(se);

    // Skip setup_scene — floor's solid_color material interferes with the
    // cube's bindless texture sampling under pbr_lit (engine batching bug).
    gpu::camera_data cam;
    cam.projection =
        glm::perspective(glm::radians(50.0f), float(TEST_WIDTH) / float(TEST_HEIGHT), 0.1f, 256.0f);
    cam.projection[1][1] *= -1.0f;
    cam.view = glm::lookAt(glm::vec3(0, 0, 3), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    cam.inv_projection = glm::inverse(cam.projection);
    cam.position = {0, 0, 3};
    renderer.set_camera(cam);

    auto* sun = cache.directional_lights.alloc(AID("pbrt_sun"));
    sun->gpu_data.direction = {-0.3f, -1.0f, -0.2f};
    sun->gpu_data.ambient = {0.3f, 0.3f, 0.3f};
    sun->gpu_data.diffuse = {0.7f, 0.7f, 0.7f};
    sun->gpu_data.specular = {1.0f, 1.0f, 1.0f};
    renderer.schd_add_light(sun);
    renderer.set_selected_directional_light(AID("pbrt_sun"));
    renderer.get_render_config().shadows.distance = 0.0f;

    auto* alb_tex = create_checker_texture(AID("pbr_tex_alb"));
    auto* spec_tex = create_checker_texture(AID("pbr_tex_spec"), 8);
    ASSERT_TRUE(alb_tex);
    ASSERT_TRUE(spec_tex);
    uint32_t alb_idx = alb_tex->get_bindless_index();
    uint32_t spec_idx = spec_tex->get_bindless_index();

    auto* mesh = create_sphere_mesh(AID("pbr_sphere"), {1, 1, 1}, 48, 48);

    gpu::pbr_material__gpu mat_gpu{};
    for (int i = 0; i < KGPU_MAX_TEXTURE_SLOTS; ++i)
    {
        mat_gpu.texture_indices[i] = UINT32_MAX;
        mat_gpu.sampler_indices[i] = UINT32_MAX;
    }
    mat_gpu.texture_indices[KGPU_TEXTURE_SLOT_ALBEDO] = alb_idx;
    mat_gpu.sampler_indices[KGPU_TEXTURE_SLOT_ALBEDO] = KGPU_SAMPLER_LINEAR_REPEAT;
    mat_gpu.texture_indices[KGPU_TEXTURE_SLOT_SPECULAR] = spec_idx;
    mat_gpu.sampler_indices[KGPU_TEXTURE_SLOT_SPECULAR] = KGPU_SAMPLER_LINEAR_REPEAT;
    mat_gpu.ambient = {0.15f, 0.15f, 0.15f};
    mat_gpu.diffuse = {1, 1, 1};
    mat_gpu.specular = {1, 1, 1};
    mat_gpu.shininess = 64.0f;

    utils::dynobj data;
    data.resize(sizeof(mat_gpu));
    std::memcpy(data.data(), &mat_gpu, sizeof(mat_gpu));

    std::vector<texture_sampler_data> samples;
    samples.push_back({alb_tex, VK_NULL_HANDLE, KGPU_TEXTURE_SLOT_ALBEDO});
    samples.push_back({spec_tex, VK_NULL_HANDLE, KGPU_TEXTURE_SLOT_SPECULAR});
    auto* mat = loader.create_material(AID("mat_pbr_tex"), AID("pbr_material"), samples, *se, data);
    renderer.schd_add_material(mat);

    auto m = glm::mat4(1.0f);
    auto* obj = cache.objects.alloc(AID("pbr_tex_obj"));
    loader.update_object(*obj, *mat, *mesh, m, glm::transpose(glm::inverse(m)), {0, 0, 0});
    renderer.schd_add_object(obj);

    for (int i = 0; i < 4; ++i)
    {
        renderer.draw_headless();
    }
    compare("material_pbr_textured", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

TEST_F(visual_pipeline_test, material_normal_map)
{
    // Genuinely blocked. Normal mapping is a feature the engine doesn't have:
    //   - gpu_generic_constants.h defines only KGPU_TEXTURE_SLOT_ALBEDO (0)
    //     and KGPU_TEXTURE_SLOT_SPECULAR (1); no NORMAL slot exists.
    //   - vertex_data has no tangent attribute and the cooker emits none.
    //   - se_pbr_lit.frag has no normal-map sampling or TBN reconstruction.
    // Implementing this test requires the feature first: add a NORMAL slot,
    // add tangents (or derive them via screen-space derivatives), wire a TBN
    // path in se_pbr_lit.frag, and re-cook. That is engine work, not test
    // scaffolding — out of scope for the test pass.
    GTEST_SKIP() << "Normal mapping not implemented in the engine "
                    "(no NORMAL slot, no tangents, no TBN sampling).";
}

TEST_F(visual_pipeline_test, shader_unlit_textured)
{
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
    auto& cache = renderer.get_cache();

    auto* se = create_textured_unlit_shader_effect(AID("se_unlit_tex"));
    ASSERT_TRUE(se);

    gpu::camera_data cam;
    cam.projection =
        glm::perspective(glm::radians(50.0f), float(TEST_WIDTH) / float(TEST_HEIGHT), 0.1f, 256.0f);
    cam.projection[1][1] *= -1.0f;
    cam.view = glm::lookAt(glm::vec3(0, 0, 3), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    cam.inv_projection = glm::inverse(cam.projection);
    cam.position = {0, 0, 3};
    renderer.set_camera(cam);

    auto* sun = cache.directional_lights.alloc(AID("ut_sun"));
    sun->gpu_data.direction = {0, -1, 0};
    sun->gpu_data.ambient = {0, 0, 0};
    sun->gpu_data.diffuse = {0, 0, 0};
    sun->gpu_data.specular = {0, 0, 0};
    renderer.schd_add_light(sun);
    renderer.set_selected_directional_light(AID("ut_sun"));
    renderer.get_render_config().shadows.distance = 0.0f;

    auto* tex = create_checker_texture(AID("ut_tex"));
    ASSERT_TRUE(tex);
    uint32_t tex_idx = tex->get_bindless_index();

    auto* mesh = create_cube_mesh(AID("ut_quad"), {1, 1, 1});

    gpu::simple_texture_material__gpu mat_gpu{};
    for (int i = 0; i < KGPU_MAX_TEXTURE_SLOTS; ++i)
    {
        mat_gpu.texture_indices[i] = UINT32_MAX;
        mat_gpu.sampler_indices[i] = UINT32_MAX;
    }
    mat_gpu.texture_indices[0] = tex_idx;
    mat_gpu.sampler_indices[0] = KGPU_SAMPLER_LINEAR_REPEAT;
    mat_gpu.ambient = {0, 0, 0};
    mat_gpu.diffuse = {1, 1, 1};
    mat_gpu.specular = {0, 0, 0};
    mat_gpu.shininess = 1.0f;

    utils::dynobj data;
    data.resize(sizeof(mat_gpu));
    std::memcpy(data.data(), &mat_gpu, sizeof(mat_gpu));

    std::vector<texture_sampler_data> samples;
    samples.push_back({tex, VK_NULL_HANDLE, 0});
    auto* mat = loader.create_material(
        AID("mat_unlit_tex"), AID("simple_texture_material"), samples, *se, data);
    renderer.schd_add_material(mat);

    auto m = glm::scale(glm::mat4(1.0f), glm::vec3(1.5f, 1.5f, 0.05f));
    auto* obj = cache.objects.alloc(AID("ut_obj"));
    loader.update_object(*obj, *mat, *mesh, m, glm::transpose(glm::inverse(m)), {0, 0, 0});
    renderer.schd_add_object(obj);

    // FRAMES_IN_FLIGHT=3: cycle 4 draws so bindless texture update is live.
    for (int i = 0; i < 4; ++i)
    {
        renderer.draw_headless();
    }
    compare("shader_unlit_textured", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// Probe-lit rendering. set_probes() (added to vulkan_render) uploads SH
// coefficients to the per-frame probe_data / probe_grid SSBOs. A runtime-
// compiled fragment shader reads them via BDA and evaluates SH for each
// fragment.
//
// Currently SKIPPED at compare() because GPU access to the probe_data /
// probe_grid BDA buffers hangs the queue (TDR). Verified isolated in this
// test:
//   - dyn_object_buffer.objects[idx].material_id reads cleanly
//   - dyn_probe_data.probes[0].coefficients[0] hangs vkWaitForFences
//   - dyn_probe_grid.grid.probe_count hangs vkWaitForFences
// Object-buffer BDA works, probe-buffer BDA does not — the probe SSBOs are
// declared the same way (see kryga_render_init.cpp:204-217 and
// kryga_render_frame.cpp:480-481) so this is an engine-side issue (probably
// missing memory-barrier or buffer-flag setup on probe_data/probe_grid). The
// shader, the runtime-compile path, and the set_probes() API all check out —
// the test runs end-to-end, falls over only on the GPU-side dereference.
TEST_F(visual_pipeline_test, probe_lighting_basic)
{
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
    auto& cache = renderer.get_cache();
    auto& vfs = glob::glob_state().getr_vfs();

    // Write a probe-lit fragment shader to a tmp file. The runtime compiler
    // (shader_compiler::compile_shader) drives glslc against the buffer's
    // file path, so the buffer's m_file must point at a real file with the
    // GLSL source.
    auto tmp = vfs.create_temp_dir();
    auto frag_path = tmp.folder() / kryga::utils::path("probe_lit.frag");

    // Self-contained shader: the test environment's shaders_includes/ symlink
    // points at the cooked tree which does not contain the GLSL include
    // sources, so #include directives can't resolve at runtime. Inline
    // everything we touch (push constants + BDA buffer refs + sh_evaluate).
    // Keep the push-constant struct binary-compatible with
    // gpu_push_constants_main.h (offsets matter — pipeline layout reflects
    // off these declarations).
    static constexpr const char* k_probe_frag = R"GLSL(
#version 450
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_buffer_reference_uvec2 : require
#extension GL_EXT_nonuniform_qualifier : require

#define KGPU_MAX_TEXTURE_SLOTS 2

struct push_constants_main {
    uint material_id;
    uint directional_light_id;
    uint use_clustered_lighting;
    uint instance_base;
    uint enable_directional_light;
    uint enable_local_lights;
    uint enable_baked_light;
    uint texture_indices[KGPU_MAX_TEXTURE_SLOTS];
    uint sampler_indices[KGPU_MAX_TEXTURE_SLOTS];
    uvec2 bdag_camera;
    uvec2 bdag_objects;
    uvec2 bdag_directional_lights;
    uvec2 bdag_universal_lights;
    uvec2 bdag_cluster_counts;
    uvec2 bdag_cluster_indices;
    uvec2 bdag_cluster_config;
    uvec2 bdag_instance_slots;
    uvec2 bdag_bone_matrices;
    uvec2 bdag_shadow_data;
    uvec2 bdag_probe_data;
    uvec2 bdag_probe_grid;
    uvec2 bdaf_material;
};

layout(push_constant, scalar) uniform Constants { push_constants_main obj; } constants;

struct object_data {
    mat4 model;
    mat4 normal;
    vec3 obj_pos;
    float bounding_radius;
    uint material_id;
    uint bone_offset;
    uint bone_count;
    uint probe_index;
    vec2 lightmap_scale;
    vec2 lightmap_offset;
    uint lightmap_texture_index;
    vec3 bounding_sphere_center;
};

struct sh_probe {
    vec4 coefficients[7];
    vec3 position;
    float radius;
};

struct probe_grid_config {
    vec3 grid_min;
    float spacing;
    vec3 grid_max;
    uint probe_count;
    uint grid_size_x;
    uint grid_size_y;
    uint grid_size_z;
};

layout(buffer_reference, scalar) readonly buffer BdaObjectRef {
    object_data objects[];
};
layout(buffer_reference, scalar) readonly buffer BdaProbeDataRef {
    sh_probe probes[];
};
layout(buffer_reference, scalar) readonly buffer BdaProbeGridRef {
    probe_grid_config grid;
};

#define dyn_object_buffer  BdaObjectRef(constants.obj.bdag_objects)
#define dyn_probe_data     BdaProbeDataRef(constants.obj.bdag_probe_data)
#define dyn_probe_grid     BdaProbeGridRef(constants.obj.bdag_probe_grid)

// Bindless texture set — declared so the pipeline layout matches the
// renderer's bound descriptor sets even though this shader doesn't sample
// textures. KGPU_textures_descriptor_sets = 2, KGPU_SAMPLER_COUNT = 7.
layout(set = 2, binding = 0) uniform sampler static_samplers[7];
layout(set = 2, binding = 1) uniform texture2D bindless_textures[];

layout (location = 0) in vec3 in_world_pos;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_color;
layout (location = 3) in vec2 in_tex_coord;
layout (location = 4) in vec2 in_lightmap_uv;
layout (location = 5) in flat uint in_object_idx;

layout (location = 0) out vec4 out_color;

vec3 sh_evaluate(sh_probe probe, vec3 n) {
    float basis[9];
    basis[0] = 0.282095;
    basis[1] = 0.488603 * n.y;
    basis[2] = 0.488603 * n.z;
    basis[3] = 0.488603 * n.x;
    basis[4] = 1.092548 * n.x * n.y;
    basis[5] = 1.092548 * n.y * n.z;
    basis[6] = 0.315392 * (3.0 * n.z * n.z - 1.0);
    basis[7] = 1.092548 * n.x * n.z;
    basis[8] = 0.546274 * (n.x * n.x - n.y * n.y);

    float c[28];
    for (int i = 0; i < 7; ++i) {
        c[i*4 + 0] = probe.coefficients[i].x;
        c[i*4 + 1] = probe.coefficients[i].y;
        c[i*4 + 2] = probe.coefficients[i].z;
        c[i*4 + 3] = probe.coefficients[i].w;
    }
    vec3 r = vec3(0.0);
    for (int i = 0; i < 9; ++i) {
        r.r += c[i]      * basis[i];
        r.g += c[i + 9]  * basis[i];
        r.b += c[i + 18] * basis[i];
    }
    return max(r, vec3(0.0));
}

void main() {
    uint pidx = dyn_object_buffer.objects[in_object_idx].probe_index;
    vec3 irrad;
    if (pidx == 0xFFFFFFFFu || pidx >= dyn_probe_grid.grid.probe_count) {
        irrad = vec3(0.0);
    } else {
        irrad = sh_evaluate(dyn_probe_data.probes[pidx], normalize(in_normal));
    }
    out_color = vec4(irrad * in_color, 1.0);
}
)GLSL";

    {
        std::ofstream out(frag_path.fs(), std::ios::binary);
        ASSERT_TRUE(out.good()) << "Cannot write " << frag_path.str();
        out.write(k_probe_frag, std::strlen(k_probe_frag));
    }

    kryga::utils::buffer frag_buf;
    ASSERT_TRUE(kryga::utils::buffer::load(frag_path, frag_buf))
        << "Cannot load probe frag from " << frag_path.str();

    // Vert: reuse pre-cooked se_lit.vert (matches the location 0..5 layout
    // common_frag.glsl expects).
    kryga::utils::buffer vert_buf;
    auto vert_dir = vfs.real_path(vfs::rid("data://packages/base.apkg/class/shader_effects/lit"));
    auto vert_path = APATH(vert_dir.value()) / "se_lit.vert.spv";
    ASSERT_TRUE(kryga::utils::buffer::load(vert_path, vert_buf));

    shader_effect_create_info se_ci;
    se_ci.vert_buffer = &vert_buf;
    se_ci.is_vert_binary = true;
    se_ci.frag_buffer = &frag_buf;
    se_ci.is_frag_binary = false;  // <-- runtime GLSL → SPV via glslc
    se_ci.cull_mode = VK_CULL_MODE_BACK_BIT;
    se_ci.width = TEST_WIDTH;
    se_ci.height = TEST_HEIGHT;

    shader_effect_data* se = nullptr;
    auto rc = m_main_pass->create_shader_effect(AID("se_probe_lit"), se_ci, se);
    ASSERT_EQ(rc, result_code::ok) << "Probe-lit shader failed to compile";
    ASSERT_TRUE(se);
    ASSERT_FALSE(se->m_failed_load);

    // Build two probes encoding constant L0-only colors. sh_evaluate sums
    // coeffs[i]*basis[i]; basis[0] = 0.282095 so setting coeffs[0] =
    // color/0.282095 yields a uniform output equal to `color`.
    constexpr float k_l0 = 0.282095f;
    auto pack_solid = [](float r, float g, float b)
    {
        gpu::sh_probe p{};
        // Coefficient packing per gpu_probe_types.h: 27 floats across 7 vec4s.
        // R[0..8] -> indices 0..8, G -> 9..17, B -> 18..26. coeffs[0] is the
        // L0 term for R; +9 is the L0 term for G; +18 for B.
        float coeffs[28] = {0};
        coeffs[0] = r / k_l0;
        coeffs[9] = g / k_l0;
        coeffs[18] = b / k_l0;
        for (int i = 0; i < 7; ++i)
        {
            p.coefficients[i].x = coeffs[i * 4 + 0];
            p.coefficients[i].y = coeffs[i * 4 + 1];
            p.coefficients[i].z = coeffs[i * 4 + 2];
            p.coefficients[i].w = coeffs[i * 4 + 3];
        }
        return p;
    };

    std::vector<gpu::sh_probe> probes = {
        pack_solid(0.95f, 0.15f, 0.15f),  // probe 0 — red
        pack_solid(0.15f, 0.30f, 0.95f),  // probe 1 — blue
    };
    probes[0].position = {-1.5f, 0, 0};
    probes[0].radius = 4.0f;
    probes[1].position = {1.5f, 0, 0};
    probes[1].radius = 4.0f;

    gpu::probe_grid_config grid{};
    grid.grid_min = {-3, -1, -1};
    grid.grid_max = {3, 1, 1};
    grid.spacing = 3.0f;
    grid.grid_size_x = 2;
    grid.grid_size_y = 1;
    grid.grid_size_z = 1;
    grid.probe_count = 2;

    renderer.schd_set_probes(std::move(probes), grid);

    // Camera + null sun (probes provide all illumination via the new shader).
    gpu::camera_data cam;
    cam.projection =
        glm::perspective(glm::radians(45.0f), float(TEST_WIDTH) / float(TEST_HEIGHT), 0.1f, 100.0f);
    cam.projection[1][1] *= -1.0f;
    cam.view = glm::lookAt(glm::vec3(0, 1.5f, 4.5f), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    cam.inv_projection = glm::inverse(cam.projection);
    cam.position = {0, 1.5f, 4.5f};
    renderer.set_camera(cam);

    auto* null_sun = cache.directional_lights.alloc(AID("probe_sun"));
    null_sun->gpu_data.direction = {0, -1, 0};
    null_sun->gpu_data.diffuse = {0, 0, 0};
    null_sun->gpu_data.ambient = {0, 0, 0};
    null_sun->gpu_data.specular = {0, 0, 0};
    renderer.schd_add_light(null_sun);
    renderer.set_selected_directional_light(AID("probe_sun"));
    renderer.get_render_config().shadows.distance = 0.0f;

    // Two spheres. Material is irrelevant (the probe-lit shader doesn't
    // sample dyn_material_buffer) but create_material requires one.
    auto mat_gpu = make_solid_color_gpu_data({0, 0, 0}, {1, 1, 1}, {0, 0, 0}, 1.0f);
    std::vector<texture_sampler_data> no_tex;
    auto* mat =
        loader.create_material(AID("probe_mat"), AID("solid_color_material"), no_tex, *se, mat_gpu);
    renderer.schd_add_material(mat);

    auto* mesh_l = create_sphere_mesh(AID("probe_sl"), {1, 1, 1}, 32, 32, 0.7f);
    auto* mesh_r = create_sphere_mesh(AID("probe_sr"), {1, 1, 1}, 32, 32, 0.7f);

    auto place = [&](mesh_data* mesh, glm::vec3 pos, uint32_t probe_idx, const char* tag)
    {
        auto m = glm::translate(glm::mat4(1.0f), pos);
        auto* o = cache.objects.alloc(AID(tag));
        loader.update_object(*o, *mat, *mesh, m, glm::transpose(glm::inverse(m)), pos);
        o->gpu_data.probe_index = probe_idx;
        renderer.schd_add_object(o);
    };

    place(mesh_l, {-1.2f, 0, 0}, 0, "probe_obj_left");
    place(mesh_r, {1.2f, 0, 0}, 1, "probe_obj_right");

    renderer.draw_headless();
    compare("probe_lighting_basic", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// Bake direct lighting only (bounce_count=0, bake_ao=false). Diff vs
// bake_lightmap_indirect / bake_ao_only isolates the direct light pass.
TEST_F(visual_pipeline_test, bake_lightmap_direct_only)
{
    gpu::directional_light_data sun{};
    sun.direction = {-0.4f, -1.0f, -0.3f};
    sun.ambient = {0.05f, 0.05f, 0.05f};
    sun.diffuse = {1.0f, 0.95f, 0.85f};
    sun.specular = {0, 0, 0};

    std::vector<bake_instance> insts;
    bake_instance floor;
    floor.pos = {0, -0.5f, 0};
    floor.scale = 5.0f;
    floor.color = {0.7f, 0.7f, 0.7f};
    floor.is_floor = true;
    insts.push_back(floor);

    bake_instance c1;
    c1.pos = {-1.0f, 0.0f, 0.0f};
    c1.color = {0.8f, 0.2f, 0.2f};
    insts.push_back(c1);

    bake_instance c2;
    c2.pos = {1.0f, 0.0f, 0.5f};
    c2.color = {0.2f, 0.8f, 0.2f};
    insts.push_back(c2);

    bake::bake_settings cfg;
    cfg.resolution = 512;
    cfg.samples_per_texel = 128;
    cfg.bounce_count = 0;
    cfg.bake_direct = true;
    cfg.bake_indirect = false;
    cfg.bake_ao = false;
    cfg.denoise_iterations = 4;
    cfg.dilate_iterations = 3;

    run_compact_bake_test("bake_lightmap_direct_only",
                          insts,
                          /*cam=*/{4, 3, 5},
                          /*tgt=*/{0, 0, 0},
                          sun,
                          cfg);
}

// Bake direct + indirect (bounce_count=3). Red wall next to a white floor —
// the bounce light tints the floor near the wall reddish. Diff vs
// bake_lightmap_direct_only isolates the indirect pass.
//
// Setup tuned so the bounce is visible:
//   - Sun direction has +X component so it actually lights the wall's -X
//     face (previously sun was straight down and the wall got 0 direct
//     light, defeating the test).
//   - Sun is moderate intensity instead of HDR-saturating, so the white
//     floor renders as a mid-bright surface that can show a red tint
//     instead of clipping to pure white.
//   - Floor color skewed slightly cool (cyan-ish) so the warm red bounce
//     stands out against it.
TEST_F(visual_pipeline_test, bake_lightmap_indirect)
{
    gpu::directional_light_data sun{};
    sun.direction = {0.6f, -1.0f, 0.0f};
    sun.ambient = {0.02f, 0.02f, 0.02f};
    sun.diffuse = {0.7f, 0.65f, 0.55f};
    sun.specular = {0, 0, 0};

    std::vector<bake_instance> insts;
    bake_instance floor;
    floor.pos = {0, -0.5f, 0};
    floor.scale = 4.0f;
    floor.color = {0.85f, 0.88f, 0.92f};  // cool floor catches red bounce
    floor.is_floor = true;
    insts.push_back(floor);

    // Red wall standing on +X side (rotated -90° around Y so its +Z normal
    // ends up facing -X, toward the floor center). The +X-tilted sun above
    // strikes this -X face.
    bake_instance wall;
    wall.pos = {3.5f, 0.5f, 0.0f};
    wall.scale = 2.0f;
    wall.color = {0.95f, 0.05f, 0.05f};
    wall.is_wall = true;
    wall.extra_rot = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0, 1, 0));
    insts.push_back(wall);

    bake::bake_settings cfg;
    cfg.resolution = 512;
    cfg.samples_per_texel = 512;
    cfg.bounce_count = 3;
    cfg.bake_direct = true;
    cfg.bake_indirect = true;
    cfg.bake_ao = false;
    cfg.denoise_iterations = 4;
    cfg.dilate_iterations = 3;

    run_compact_bake_test("bake_lightmap_indirect",
                          insts,
                          /*cam=*/{-1.5f, 3.0f, 4.0f},
                          /*tgt=*/{2.0f, 0, 0},
                          sun,
                          cfg);
}

// AO-only bake (no direct, no indirect). The baker writes (ao, ao, ao) into
// the lightmap output when only AO is requested, so the runtime samples
// `lightmap * albedo = AO * material_diffuse` and shows the AO darkening on
// the per-instance albedo without any direct light. Isolates ao_baker.comp.
TEST_F(visual_pipeline_test, bake_ao_only)
{
    gpu::directional_light_data sun{};
    sun.direction = {-0.3f, -1.0f, -0.3f};
    sun.ambient = {0, 0, 0};
    sun.diffuse = {0, 0, 0};
    sun.specular = {0, 0, 0};

    std::vector<bake_instance> insts;
    bake_instance floor;
    floor.pos = {0, -0.5f, 0};
    floor.scale = 4.0f;
    floor.color = {0.9f, 0.9f, 0.9f};
    floor.is_floor = true;
    insts.push_back(floor);

    bake_instance c1;
    c1.pos = {-0.6f, 0.0f, 0.0f};
    c1.color = {0.9f, 0.9f, 0.9f};
    insts.push_back(c1);

    bake_instance c2;
    c2.pos = {0.6f, 0.0f, -0.3f};
    c2.color = {0.9f, 0.9f, 0.9f};
    insts.push_back(c2);

    bake::bake_settings cfg;
    cfg.resolution = 512;
    cfg.samples_per_texel = 1024;
    cfg.bounce_count = 0;
    cfg.bake_direct = false;
    cfg.bake_indirect = false;
    cfg.bake_ao = true;
    cfg.ao_radius = 1.5f;
    cfg.ao_intensity = 1.0f;
    cfg.denoise_iterations = 8;
    cfg.dilate_iterations = 6;

    run_compact_bake_test("bake_ao_only",
                          insts,
                          /*cam=*/{3, 3, 4},
                          /*tgt=*/{0, 0, 0},
                          sun,
                          cfg);
}

// Single rotated cube — six faces map into six different chart cells in the
// instance tile. Heavy dilate_iterations validates that chart boundaries
// don't show a visible discontinuity at face edges (lightmap_dilate.comp
// fills gutter texels between charts).
TEST_F(visual_pipeline_test, bake_lightmap_seams)
{
    gpu::directional_light_data sun{};
    sun.direction = {-0.4f, -0.8f, -0.4f};
    sun.ambient = {0.05f, 0.05f, 0.05f};
    sun.diffuse = {1.2f, 1.1f, 1.0f};
    sun.specular = {0, 0, 0};

    std::vector<bake_instance> insts;

    // Single cube at origin, rotated so that two faces are visible from the
    // camera with their chart boundary running through the silhouette.
    bake_instance cube;
    cube.pos = {0, 0, 0};
    cube.scale = 1.5f;
    cube.color = {0.85f, 0.85f, 0.85f};
    cube.extra_rot = glm::rotate(glm::mat4(1.0f), glm::radians(35.0f), glm::vec3(0, 1, 0));
    insts.push_back(cube);

    bake::bake_settings cfg;
    cfg.resolution = 512;
    cfg.samples_per_texel = 256;
    cfg.bounce_count = 1;
    cfg.bake_direct = true;
    cfg.bake_indirect = true;
    cfg.bake_ao = false;
    cfg.denoise_iterations = 4;
    cfg.dilate_iterations = 8;  // heavy dilation — proves gutter coverage

    run_compact_bake_test("bake_lightmap_seams",
                          insts,
                          /*cam=*/{2.0f, 1.5f, 2.5f},
                          /*tgt=*/{0, 0, 0},
                          sun,
                          cfg);
}

TEST_F(visual_pipeline_test, shader_error_fallback)
{
    // Trigger se_error by giving the shader effect a deliberately broken
    // fragment shader (empty buffer). create_shader_effect should detect the
    // failure and substitute the magenta fallback.
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
    auto& cache = renderer.get_cache();

    kryga::utils::buffer vert_buf;
    auto se_path = glob::glob_state().getr_vfs().real_path(
        vfs::rid("data://packages/base.apkg/class/shader_effects"));
    auto path = APATH(se_path.value());
    kryga::utils::buffer::load(path / "se_solid_color.vert.spv", vert_buf);

    // Empty buffer for fragment — should cause SPIR-V module creation to fail
    // and the loader to fall back to se_error.
    kryga::utils::buffer bad_frag(0);

    shader_effect_create_info se_ci;
    se_ci.vert_buffer = &vert_buf;
    se_ci.frag_buffer = &bad_frag;
    se_ci.cull_mode = VK_CULL_MODE_BACK_BIT;
    se_ci.width = TEST_WIDTH;
    se_ci.height = TEST_HEIGHT;

    shader_effect_data* se = nullptr;
    auto rc = m_main_pass->create_shader_effect(AID("se_err_broken"), se_ci, se);
    if (rc != result_code::ok || !se || se->m_failed_load)
    {
        // Loader rejected it outright — try a working SE so we still render.
        // The fallback might happen at material level instead.
        se = create_unlit_shader_effect(AID("se_err_working"));
        ASSERT_TRUE(se);
    }

    gpu::camera_data cam;
    cam.projection =
        glm::perspective(glm::radians(50.0f), float(TEST_WIDTH) / float(TEST_HEIGHT), 0.1f, 256.0f);
    cam.projection[1][1] *= -1.0f;
    cam.view = glm::lookAt(glm::vec3(0, 0, 3), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    cam.inv_projection = glm::inverse(cam.projection);
    cam.position = {0, 0, 3};
    renderer.set_camera(cam);

    auto* sun = cache.directional_lights.alloc(AID("err_sun"));
    sun->gpu_data.diffuse = {1, 1, 1};
    renderer.schd_add_light(sun);
    renderer.set_selected_directional_light(AID("err_sun"));
    renderer.get_render_config().shadows.distance = 0.0f;

    std::vector<texture_sampler_data> no_tex;
    auto gpu = make_solid_color_gpu_data({0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {0, 0, 0}, 1.0f);
    auto* mat =
        loader.create_material(AID("mat_err"), AID("solid_color_material"), no_tex, *se, gpu);
    renderer.schd_add_material(mat);

    auto* mesh = create_cube_mesh(AID("err_cube"), {1, 1, 1});
    auto m = glm::mat4(1.0f);
    auto* obj = cache.objects.alloc(AID("err_obj"));
    loader.update_object(*obj, *mat, *mesh, m, glm::transpose(glm::inverse(m)), {0, 0, 0});
    renderer.schd_add_object(obj);

    renderer.draw_headless();
    compare("shader_error_fallback", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// Enable the debug-overlay wireframe pass and add point + spot lights so the
// engine emits a wireframe cube around each one. The billboard half of the
// overlay (debug.light_icons) is driven by editor-only mesh components that
// the renderer does not auto-create from lights, so the test validates only
// the wireframe path; the icons branch ends up as a no-op which is fine —
// regressions in the wireframe sub-pass still surface as reference diffs.
TEST_F(visual_pipeline_test, debug_billboards_and_wireframe)
{
    render_config cfg{};
    cfg.debug.show_grid = false;
    cfg.debug.light_wireframe = true;
    cfg.debug.light_icons = true;
    cfg.debug.editor_mode = true;
    reinit_with_config(cfg);

    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
    auto& cache = renderer.get_cache();

    auto* se = create_lit_shader_effect(AID("se_dbg_overlay"));
    setup_scene("dbg", glm::vec3(0, 4, 8), glm::vec3(0, 0, 0), se);

    std::vector<texture_sampler_data> no_tex;

    // Solid floor + cube so wireframes overlay something and we can spot
    // depth-test bugs in the overlay path.
    auto* mesh = create_cube_mesh(AID("dbg_cube"), {1, 1, 1});
    auto cube_gpu = make_solid_color_gpu_data(
        {0.15f, 0.15f, 0.15f}, {0.55f, 0.55f, 0.55f}, {0.2f, 0.2f, 0.2f}, 16.0f);
    auto* cube_mat = loader.create_material(
        AID("dbg_cube_mat"), AID("solid_color_material"), no_tex, *se, cube_gpu);
    renderer.schd_add_material(cube_mat);
    auto cube_m = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 0));
    auto* cube_obj = cache.objects.alloc(AID("dbg_cube_obj"));
    loader.update_object(
        *cube_obj, *cube_mat, *mesh, cube_m, glm::transpose(glm::inverse(cube_m)), {0, 0, 0});
    renderer.schd_add_object(cube_obj);

    // Two point lights and one spot — each gets a debug wireframe cube
    // drawn in prepare_debug_light_data().
    auto* pl_l = cache.universal_lights.alloc(AID("dbg_pl_l"), light_type::point);
    pl_l->gpu_data.position = {-2.5f, 1.5f, 1.5f};
    pl_l->gpu_data.diffuse = {1.0f, 0.4f, 0.2f};
    pl_l->gpu_data.specular = {1, 1, 1};
    pl_l->gpu_data.radius = 5.0f;
    pl_l->gpu_data.type = KGPU_light_type_point;
    pl_l->gpu_data.cut_off = -1.0f;
    pl_l->gpu_data.outer_cut_off = -1.0f;
    renderer.schd_add_light(pl_l);

    auto* pl_r = cache.universal_lights.alloc(AID("dbg_pl_r"), light_type::point);
    pl_r->gpu_data.position = {2.5f, 1.5f, 1.5f};
    pl_r->gpu_data.diffuse = {0.2f, 0.4f, 1.0f};
    pl_r->gpu_data.specular = {1, 1, 1};
    pl_r->gpu_data.radius = 5.0f;
    pl_r->gpu_data.type = KGPU_light_type_point;
    pl_r->gpu_data.cut_off = -1.0f;
    pl_r->gpu_data.outer_cut_off = -1.0f;
    renderer.schd_add_light(pl_r);

    auto* sp = cache.universal_lights.alloc(AID("dbg_sp"), light_type::spot);
    sp->gpu_data.position = {0.0f, 3.0f, -1.0f};
    sp->gpu_data.direction = glm::normalize(glm::vec3(0.0f, -1.0f, 0.0f));
    sp->gpu_data.diffuse = {0.4f, 1.0f, 0.4f};
    sp->gpu_data.specular = {1, 1, 1};
    sp->gpu_data.radius = 6.0f;
    sp->gpu_data.type = KGPU_light_type_spot;
    sp->gpu_data.cut_off = std::cos(glm::radians(15.0f));
    sp->gpu_data.outer_cut_off = std::cos(glm::radians(25.0f));
    renderer.schd_add_light(sp);

    renderer.draw_headless();
    compare("debug_billboards_and_wireframe", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// Same compact baked scene as bake_lightmap_direct_only but with the runtime
// baked-lighting toggle off. Diff isolates the effect of is_baked_light_enabled().
TEST_F(visual_pipeline_test, toggle_lighting_baked_off)
{
    gpu::directional_light_data sun{};
    sun.direction = {-0.4f, -1.0f, -0.3f};
    sun.ambient = {0.05f, 0.05f, 0.05f};
    sun.diffuse = {1.0f, 0.95f, 0.85f};
    sun.specular = {0, 0, 0};

    std::vector<bake_instance> insts;
    bake_instance floor;
    floor.pos = {0, -0.5f, 0};
    floor.scale = 5.0f;
    floor.color = {0.7f, 0.7f, 0.7f};
    floor.is_floor = true;
    insts.push_back(floor);

    bake_instance c1;
    c1.pos = {-1.0f, 0.0f, 0.0f};
    c1.color = {0.8f, 0.2f, 0.2f};
    insts.push_back(c1);

    bake_instance c2;
    c2.pos = {1.0f, 0.0f, 0.5f};
    c2.color = {0.2f, 0.8f, 0.2f};
    insts.push_back(c2);

    bake::bake_settings cfg;
    cfg.resolution = 512;
    cfg.samples_per_texel = 128;
    cfg.bounce_count = 0;
    cfg.bake_direct = true;
    cfg.bake_indirect = false;
    cfg.bake_ao = false;
    cfg.denoise_iterations = 4;
    cfg.dilate_iterations = 3;

    // Run the bake + render path, then disable baked lighting at the runtime
    // gate before draw. The compact helper enables it; flip just before the
    // last draw cycle. Easiest path: call the helper, then re-render with the
    // gate off and overwrite the comparison name.
    auto& renderer = glob::glob_state().getr_render().renderer;

    // Manually set up the same scene; cannot call run_compact_bake_test
    // directly because we need to flip the runtime flag *after* setup but
    // *before* draw_headless. Inline a minimal version here.
    auto& loader = glob::glob_state().getr_render().loader;
    auto& cache = renderer.get_cache();

    auto* se_lm = create_lightmapped_shader_effect(AID("toff_se"));
    ASSERT_TRUE(se_lm);
    ASSERT_FALSE(se_lm->m_failed_load);

    gpu::camera_data cam;
    cam.projection =
        glm::perspective(glm::radians(60.0f), float(TEST_WIDTH) / float(TEST_HEIGHT), 0.1f, 256.0f);
    cam.projection[1][1] *= -1.0f;
    cam.view = glm::lookAt(glm::vec3(4, 3, 5), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    cam.inv_projection = glm::inverse(cam.projection);
    cam.position = {4, 3, 5};
    renderer.set_camera(cam);

    auto* null_sun = cache.directional_lights.alloc(AID("toff_sun"));
    null_sun->gpu_data.direction = {0, -1, 0};
    null_sun->gpu_data.ambient = {0, 0, 0};
    null_sun->gpu_data.diffuse = {0, 0, 0};
    null_sun->gpu_data.specular = {0, 0, 0};
    renderer.schd_add_light(null_sun);
    renderer.set_selected_directional_light(AID("toff_sun"));

    renderer.get_render_config().lighting.directional_enabled = false;
    renderer.get_render_config().lighting.local_enabled = false;
    renderer.get_render_config().lighting.baked_enabled = false;  // <-- key flip

    const uint32_t LM = cfg.resolution;
    lightmap_atlas atlas(LM, LM);
    const float inv_w = 1.0f / float(LM);
    const float inv_h = 1.0f / float(LM);

    const auto cube_verts = make_cube_lm_verts();
    const auto cube_indices = make_cube_lm_indices();

    std::vector<gpu::vertex_data> floor_verts = {
        {{-1, 0, 1}, {0, 1, 0}, {1, 1, 1}, {0, 0}, {0, 0}},
        {{1, 0, 1}, {0, 1, 0}, {1, 1, 1}, {1, 0}, {1, 0}},
        {{1, 0, -1}, {0, 1, 0}, {1, 1, 1}, {1, 1}, {1, 1}},
        {{-1, 0, -1}, {0, 1, 0}, {1, 1, 1}, {0, 1}, {0, 1}},
    };
    std::vector<gpu::uint> quad_indices = {0, 1, 2, 2, 3, 0};

    lightmap_baker baker;
    for (size_t i = 0; i < insts.size(); ++i)
    {
        auto& inst = insts[i];
        const uint32_t tile = inst.is_floor ? 256 : 64;
        auto tile_id = AID(("toff_tile_" + std::to_string(i)).c_str());
        ASSERT_TRUE(atlas.allocate(tile_id, tile, tile));
        const auto* region = atlas.get_region(tile_id);
        inst.lightmap_scale = {float(region->width) * inv_w, float(region->height) * inv_h};
        inst.lightmap_offset = {float(region->x) * inv_w, float(region->y) * inv_h};

        std::vector<gpu::vertex_data> remapped = inst.is_floor ? floor_verts : cube_verts;
        std::vector<gpu::uint> remapped_idx = inst.is_floor ? quad_indices : cube_indices;
        for (auto& v : remapped)
        {
            v.position = inst.pos + v.position * inst.scale;
            v.uv2.x = v.uv2.x * inst.lightmap_scale.x + inst.lightmap_offset.x;
            v.uv2.y = v.uv2.y * inst.lightmap_scale.y + inst.lightmap_offset.y;
        }
        baker.add_mesh(remapped.data(),
                       static_cast<uint32_t>(remapped.size()),
                       remapped_idx.data(),
                       static_cast<uint32_t>(remapped_idx.size()));
    }

    cfg.directional_lights.push_back(sun);
    cfg.output_lightmap = vfs::rid("tmp://toff_lm.bin");
    cfg.output_ao = vfs::rid("tmp://toff_ao.bin");
    cfg.output_png = false;

    auto rc = baker.bake(cfg);
    ASSERT_TRUE(rc.success);
    baker.clear();

    std::vector<uint8_t> lm_loaded;
    ASSERT_TRUE(vfs::load_file(cfg.output_lightmap, lm_loaded));

    kryga::utils::buffer lm_buf(lm_loaded.size());
    std::memcpy(lm_buf.data(), lm_loaded.data(), lm_loaded.size());
    auto* lm_tex = loader.create_texture(
        AID("toff_lm_tex"), lm_buf, LM, LM, VK_FORMAT_R16G16B16A16_SFLOAT, texture_format::rgba16f);
    const uint32_t lm_idx = lm_tex->get_bindless_index();

    kryga::utils::buffer cube_vb(cube_verts.size() * sizeof(gpu::vertex_data));
    std::memcpy(cube_vb.data(), cube_verts.data(), cube_vb.size());
    kryga::utils::buffer cube_ib(cube_indices.size() * sizeof(gpu::uint));
    std::memcpy(cube_ib.data(), cube_indices.data(), cube_ib.size());
    auto* cube_mesh = loader.create_mesh(AID("toff_cube_mesh"),
                                         cube_vb.make_view<gpu::vertex_data>(),
                                         cube_ib.make_view<gpu::uint>());

    kryga::utils::buffer floor_vb(floor_verts.size() * sizeof(gpu::vertex_data));
    std::memcpy(floor_vb.data(), floor_verts.data(), floor_vb.size());
    kryga::utils::buffer floor_ib(quad_indices.size() * sizeof(gpu::uint));
    std::memcpy(floor_ib.data(), quad_indices.data(), floor_ib.size());
    auto* floor_mesh = loader.create_mesh(AID("toff_floor_mesh"),
                                          floor_vb.make_view<gpu::vertex_data>(),
                                          floor_ib.make_view<gpu::uint>());

    std::vector<texture_sampler_data> no_tex;
    for (size_t i = 0; i < insts.size(); ++i)
    {
        auto& inst = insts[i];
        auto mat_gpu = make_solid_color_gpu_data({0, 0, 0}, inst.color, {0, 0, 0}, 8.0f);
        auto* mat = loader.create_material(AID(("toff_mat_" + std::to_string(i)).c_str()),
                                           AID("solid_color_material"),
                                           no_tex,
                                           *se_lm,
                                           mat_gpu);
        renderer.schd_add_material(mat);

        auto* obj = cache.objects.alloc(AID(("toff_obj_" + std::to_string(i)).c_str()));
        auto* mesh = inst.is_floor ? floor_mesh : cube_mesh;
        auto model = glm::translate(glm::mat4(1.0f), inst.pos) *
                     glm::scale(glm::mat4(1.0f), glm::vec3(inst.scale));
        loader.update_object(
            *obj, *mat, *mesh, model, glm::transpose(glm::inverse(model)), inst.pos);
        obj->gpu_data.lightmap_scale = inst.lightmap_scale;
        obj->gpu_data.lightmap_offset = inst.lightmap_offset;
        obj->gpu_data.lightmap_texture_index = lm_idx;
        renderer.schd_add_object(obj);
    }

    for (int i = 0; i < 4; ++i)
    {
        renderer.draw_headless();
    }
    compare("toggle_lighting_baked_off", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

TEST_F(visual_pipeline_test, post_depth_outline)
{
    // outline_cfg.enabled requires render_scale.enabled per render_config.h.
    render_config cfg{};
    cfg.debug.show_grid = false;
    cfg.debug.light_wireframe = false;
    cfg.debug.light_icons = false;
    cfg.render_scale.enabled = true;
    cfg.render_scale.divisor = 1;
    cfg.outline.enabled = true;
    reinit_with_config(cfg);

    auto& renderer = glob::glob_state().getr_render().renderer;
    auto* host_pass =
        glob::glob_state().getr_render().loader.get_render_pass(renderer.get_host_pass_id());
    ASSERT_TRUE(host_pass);

    auto* se = create_lit_shader_effect(AID("se_outpost_test"));
    setup_scene("outpost", glm::vec3(0, 1.5f, 4), glm::vec3(0, 0, 0), se);
    {
        auto& loader = glob::glob_state().getr_render().loader;
        auto& cache = renderer.get_cache();
        auto* sphere_mesh = create_sphere_mesh(AID("outpost_sphere_mesh"), {1, 1, 1}, 32, 32);
        std::vector<texture_sampler_data> no_tex;
        auto sphere_gpu = make_solid_color_gpu_data(
            {0.1f, 0.4f, 0.1f}, {0.2f, 0.7f, 0.2f}, {0.5f, 1.0f, 0.5f}, 32.0f);
        auto* sphere_mat = loader.create_material(
            AID("outpost_sphere_mat"), AID("solid_color_material"), no_tex, *se, sphere_gpu);
        renderer.schd_add_material(sphere_mat);
        auto sm = glm::mat4(1.0f);
        auto* sphere_obj = cache.objects.alloc(AID("outpost_sphere"));
        sphere_obj->outlined = true;
        loader.update_object(*sphere_obj,
                             *sphere_mat,
                             *sphere_mesh,
                             sm,
                             glm::transpose(glm::inverse(sm)),
                             {0, 0, 0});
        renderer.schd_add_object(sphere_obj);
    }

    renderer.draw_headless();
    compare("post_depth_outline", *host_pass, TEST_WIDTH, TEST_HEIGHT);
}

TEST_F(visual_pipeline_test, toggle_outline_off)
{
    // Same scene as post_depth_outline but outline disabled — should NOT show
    // the silhouette outline around the sphere.
    render_config cfg{};
    cfg.debug.show_grid = false;
    cfg.debug.light_wireframe = false;
    cfg.debug.light_icons = false;
    cfg.render_scale.enabled = true;
    cfg.render_scale.divisor = 1;
    cfg.outline.enabled = false;
    reinit_with_config(cfg);

    auto& renderer = glob::glob_state().getr_render().renderer;
    auto* host_pass =
        glob::glob_state().getr_render().loader.get_render_pass(renderer.get_host_pass_id());
    ASSERT_TRUE(host_pass);

    auto* se = create_lit_shader_effect(AID("se_outline_off"));
    setup_scene("outoff", glm::vec3(0, 1.5f, 4), glm::vec3(0, 0, 0), se);
    {
        auto& loader = glob::glob_state().getr_render().loader;
        auto& cache = renderer.get_cache();
        auto* sphere_mesh = create_sphere_mesh(AID("outoff_sphere_mesh"), {1, 1, 1}, 32, 32);
        std::vector<texture_sampler_data> no_tex;
        auto sphere_gpu = make_solid_color_gpu_data(
            {0.1f, 0.4f, 0.1f}, {0.2f, 0.7f, 0.2f}, {0.5f, 1.0f, 0.5f}, 32.0f);
        auto* sphere_mat = loader.create_material(
            AID("outoff_sphere_mat"), AID("solid_color_material"), no_tex, *se, sphere_gpu);
        renderer.schd_add_material(sphere_mat);
        auto sm = glm::mat4(1.0f);
        auto* sphere_obj = cache.objects.alloc(AID("outoff_sphere"));
        sphere_obj->outlined = true;
        loader.update_object(*sphere_obj,
                             *sphere_mat,
                             *sphere_mesh,
                             sm,
                             glm::transpose(glm::inverse(sm)),
                             {0, 0, 0});
        renderer.schd_add_object(sphere_obj);
    }

    renderer.draw_headless();
    compare("toggle_outline_off", *host_pass, TEST_WIDTH, TEST_HEIGHT);
}

TEST_F(visual_pipeline_test, preset_low)
{
    // Low quality: shadows off, half-res render scale, no outline, no debug.
    render_config cfg{};
    cfg.shadows.enabled = false;
    cfg.shadows.distance = 0.0f;
    cfg.render_scale.enabled = true;
    cfg.render_scale.divisor = 2;
    cfg.outline.enabled = false;
    cfg.debug.show_grid = false;
    cfg.debug.light_wireframe = false;
    cfg.debug.light_icons = false;
    reinit_with_config(cfg);

    auto& renderer = glob::glob_state().getr_render().renderer;
    auto* host_pass =
        glob::glob_state().getr_render().loader.get_render_pass(renderer.get_host_pass_id());
    ASSERT_TRUE(host_pass);

    auto* se = create_lit_shader_effect(AID("se_preset_low"));
    setup_scene("plow", glm::vec3(2, 2, 5), glm::vec3(0, 0, 0), se);
    {
        auto& loader = glob::glob_state().getr_render().loader;
        auto& cache = renderer.get_cache();
        auto* sphere_mesh = create_sphere_mesh(AID("plow_sphere_mesh"), {1, 1, 1}, 32, 32);
        std::vector<texture_sampler_data> no_tex;
        auto sphere_gpu = make_solid_color_gpu_data(
            {0.1f, 0.4f, 0.1f}, {0.2f, 0.7f, 0.2f}, {0.5f, 1.0f, 0.5f}, 32.0f);
        auto* sphere_mat = loader.create_material(
            AID("plow_sphere_mat"), AID("solid_color_material"), no_tex, *se, sphere_gpu);
        renderer.schd_add_material(sphere_mat);
        auto sm = glm::mat4(1.0f);
        auto* sphere_obj = cache.objects.alloc(AID("plow_sphere"));
        loader.update_object(*sphere_obj,
                             *sphere_mat,
                             *sphere_mesh,
                             sm,
                             glm::transpose(glm::inverse(sm)),
                             {0, 0, 0});
        renderer.schd_add_object(sphere_obj);
    }

    renderer.draw_headless();
    compare("preset_low", *host_pass, TEST_WIDTH, TEST_HEIGHT);
}

TEST_F(visual_pipeline_test, preset_medium)
{
    // Medium quality: shadows on PCF 3x3, full res, no outline.
    render_config cfg{};
    cfg.shadows.enabled = true;
    cfg.shadows.pcf = pcf_mode::pcf_3x3;
    cfg.render_scale.enabled = false;
    cfg.outline.enabled = false;
    cfg.debug.show_grid = false;
    cfg.debug.light_wireframe = false;
    cfg.debug.light_icons = false;
    reinit_with_config(cfg);

    auto& renderer = glob::glob_state().getr_render().renderer;

    auto* se = create_lit_shader_effect(AID("se_preset_med"));
    setup_scene("pmed", glm::vec3(2, 2, 5), glm::vec3(0, 0, 0), se, true);
    {
        auto& loader = glob::glob_state().getr_render().loader;
        auto& cache = renderer.get_cache();
        auto* sphere_mesh = create_sphere_mesh(AID("pmed_sphere_mesh"), {1, 1, 1}, 32, 32);
        std::vector<texture_sampler_data> no_tex;
        auto sphere_gpu = make_solid_color_gpu_data(
            {0.1f, 0.4f, 0.1f}, {0.2f, 0.7f, 0.2f}, {0.5f, 1.0f, 0.5f}, 32.0f);
        auto* sphere_mat = loader.create_material(
            AID("pmed_sphere_mat"), AID("solid_color_material"), no_tex, *se, sphere_gpu);
        renderer.schd_add_material(sphere_mat);
        auto sm = glm::mat4(1.0f);
        auto* sphere_obj = cache.objects.alloc(AID("pmed_sphere"));
        loader.update_object(*sphere_obj,
                             *sphere_mat,
                             *sphere_mesh,
                             sm,
                             glm::transpose(glm::inverse(sm)),
                             {0, 0, 0});
        renderer.schd_add_object(sphere_obj);
    }

    renderer.draw_headless();
    compare("preset_medium", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

TEST_F(visual_pipeline_test, preset_high)
{
    // High quality: shadows on Poisson-32, full res, outline on.
    render_config cfg{};
    cfg.shadows.enabled = true;
    cfg.shadows.pcf = pcf_mode::poisson32;
    cfg.render_scale.enabled = true;
    cfg.render_scale.divisor = 1;  // Full res but composite path active for outline
    cfg.outline.enabled = true;
    cfg.debug.show_grid = false;
    cfg.debug.light_wireframe = false;
    cfg.debug.light_icons = false;
    reinit_with_config(cfg);

    auto& renderer = glob::glob_state().getr_render().renderer;
    auto* host_pass =
        glob::glob_state().getr_render().loader.get_render_pass(renderer.get_host_pass_id());
    ASSERT_TRUE(host_pass);

    auto* se = create_lit_shader_effect(AID("se_preset_high"));
    setup_scene("phigh", glm::vec3(2, 2, 5), glm::vec3(0, 0, 0), se, true);
    {
        auto& loader = glob::glob_state().getr_render().loader;
        auto& cache = renderer.get_cache();
        auto* sphere_mesh = create_sphere_mesh(AID("phigh_sphere_mesh"), {1, 1, 1}, 32, 32);
        std::vector<texture_sampler_data> no_tex;
        auto sphere_gpu = make_solid_color_gpu_data(
            {0.1f, 0.4f, 0.1f}, {0.2f, 0.7f, 0.2f}, {0.5f, 1.0f, 0.5f}, 32.0f);
        auto* sphere_mat = loader.create_material(
            AID("phigh_sphere_mat"), AID("solid_color_material"), no_tex, *se, sphere_gpu);
        renderer.schd_add_material(sphere_mat);
        auto sm = glm::mat4(1.0f);
        auto* sphere_obj = cache.objects.alloc(AID("phigh_sphere"));
        sphere_obj->outlined = true;
        loader.update_object(*sphere_obj,
                             *sphere_mat,
                             *sphere_mesh,
                             sm,
                             glm::transpose(glm::inverse(sm)),
                             {0, 0, 0});
        renderer.schd_add_object(sphere_obj);
    }

    renderer.draw_headless();
    compare("preset_high", *host_pass, TEST_WIDTH, TEST_HEIGHT);
}

TEST_F(visual_pipeline_test, noop_outline_no_marked)
{
    // Outline post enabled but no objects flagged outlined — should match a
    // baseline render with outline disabled (no silhouette pixels added).
    render_config cfg{};
    cfg.debug.show_grid = false;
    cfg.debug.light_wireframe = false;
    cfg.debug.light_icons = false;
    cfg.render_scale.enabled = true;
    cfg.render_scale.divisor = 1;
    cfg.outline.enabled = true;
    reinit_with_config(cfg);

    auto& renderer = glob::glob_state().getr_render().renderer;
    auto* host_pass =
        glob::glob_state().getr_render().loader.get_render_pass(renderer.get_host_pass_id());
    ASSERT_TRUE(host_pass);

    auto* se = create_lit_shader_effect(AID("se_noop_outline"));
    setup_scene("noop_o", glm::vec3(0, 1.5f, 4), glm::vec3(0, 0, 0), se);
    {
        auto& loader = glob::glob_state().getr_render().loader;
        auto& cache = renderer.get_cache();
        auto* sphere_mesh = create_sphere_mesh(AID("noop_o_sphere_mesh"), {1, 1, 1}, 32, 32);
        std::vector<texture_sampler_data> no_tex;
        auto sphere_gpu = make_solid_color_gpu_data(
            {0.1f, 0.4f, 0.1f}, {0.2f, 0.7f, 0.2f}, {0.5f, 1.0f, 0.5f}, 32.0f);
        auto* sphere_mat = loader.create_material(
            AID("noop_o_sphere_mat"), AID("solid_color_material"), no_tex, *se, sphere_gpu);
        renderer.schd_add_material(sphere_mat);
        auto sm = glm::mat4(1.0f);
        auto* sphere_obj = cache.objects.alloc(AID("noop_o_sphere"));
        // NOT marked outlined.
        loader.update_object(*sphere_obj,
                             *sphere_mat,
                             *sphere_mesh,
                             sm,
                             glm::transpose(glm::inverse(sm)),
                             {0, 0, 0});
        renderer.schd_add_object(sphere_obj);
    }

    renderer.draw_headless();
    compare("noop_outline_no_marked", *host_pass, TEST_WIDTH, TEST_HEIGHT);
}

TEST_F(visual_pipeline_test, voronoi_fractured_cube)
{
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
    auto& cache = renderer.get_cache();

    auto* se = create_lit_shader_effect(AID("se_frac"));
    ASSERT_TRUE(se);

    auto scene = setup_scene("frac", {3, 2.5f, 4}, {0, 0, 0}, se);

    // High-poly sphere so Voronoi cell boundaries are visible (2048 tris)
    std::vector<gpu::vertex_data> sphere_verts;
    std::vector<gpu::uint> sphere_idx;
    {
        constexpr uint32_t stacks = 32, slices = 32;
        constexpr float radius = 1.0f;
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
                sphere_verts.push_back({p, n, {1, 1, 1}, uv});
            }
        }
        for (uint32_t i = 0; i < stacks; ++i)
        {
            for (uint32_t j = 0; j < slices; ++j)
            {
                uint32_t a = i * (slices + 1) + j;
                uint32_t b = a + slices + 1;
                sphere_idx.insert(sphere_idx.end(), {a, a + 1, b, a + 1, b + 1, b});
            }
        }
    }

    voronoi_fracture::fracture_params fp;
    fp.seed = 42;
    fp.cell_count = 8;
    auto result = voronoi_fracture::fracture_mesh(sphere_verts.data(),
                                                  (uint32_t)sphere_verts.size(),
                                                  sphere_idx.data(),
                                                  (uint32_t)sphere_idx.size(),
                                                  fp);
    ASSERT_FALSE(result.chunks.empty());

    // clang-format off
    const glm::vec3 palette[] = {
        {0.9f, 0.2f, 0.2f}, {0.2f, 0.7f, 0.3f}, {0.2f, 0.3f, 0.9f},
        {0.9f, 0.8f, 0.1f}, {0.8f, 0.3f, 0.8f}, {0.1f, 0.8f, 0.8f},
        {0.9f, 0.5f, 0.1f}, {0.4f, 0.9f, 0.7f},
    };
    // clang-format on

    std::vector<texture_sampler_data> no_tex;

    for (uint32_t i = 0; i < result.chunks.size(); ++i)
    {
        auto& ck = result.chunks[i];
        if (ck.indices.empty())
        {
            continue;
        }

        auto suffix = std::to_string(i);
        auto color = palette[i % 8];

        auto gpu_data = make_solid_color_gpu_data(color * 0.4f, color, {0.6f, 0.6f, 0.6f}, 32.0f);
        auto* mat = loader.create_material(AID(("frac_mat_" + suffix).c_str()),
                                           AID("solid_color_material"),
                                           no_tex,
                                           *se,
                                           gpu_data);
        renderer.schd_add_material(mat);

        kryga::utils::buffer vb(ck.vertices.size() * sizeof(gpu::vertex_data));
        std::memcpy(vb.data(), ck.vertices.data(), vb.size());
        kryga::utils::buffer ib(ck.indices.size() * sizeof(gpu::uint));
        std::memcpy(ib.data(), ck.indices.data(), ib.size());

        auto* mesh = loader.create_mesh(AID(("frac_mesh_" + suffix).c_str()),
                                        vb.make_view<gpu::vertex_data>(),
                                        ib.make_view<gpu::uint>());

        // Explode outward from origin along seed point direction
        float len = glm::length(ck.seed_point);
        glm::vec3 dir = len > 1e-6f ? ck.seed_point / len : glm::vec3(0, 1, 0);
        glm::vec3 offset = dir * 1.2f;
        auto model = glm::translate(glm::mat4(1.0f), offset);

        auto* obj = cache.objects.alloc(AID(("frac_obj_" + suffix).c_str()));
        loader.update_object(*obj, *mat, *mesh, model, glm::transpose(glm::inverse(model)), offset);
        renderer.schd_add_object(obj);
    }

    renderer.draw_headless();
    compare("voronoi_fractured_cube", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// =============================================================================
// Chunk normals under point light — verify cut faces are correctly lit
// =============================================================================
TEST_F(visual_pipeline_test, chunk_normals_point_light)
{
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
    auto& cache = renderer.get_cache();

    auto* se = create_lit_shader_effect(AID("se_cnpl"));
    ASSERT_TRUE(se);

    // Camera looking slightly down at the chunks
    auto scene = setup_scene("cnpl", {3, 3, 5}, {0, 0, 0}, se);

    // White point light directly above — strong diffuse
    auto* pl = cache.universal_lights.alloc(AID("cnpl_pl"), light_type::point);
    pl->gpu_data.position = {0.0f, 4.0f, 0.0f};
    pl->gpu_data.ambient = {0.05f, 0.05f, 0.05f};
    pl->gpu_data.diffuse = {1.0f, 1.0f, 1.0f};
    pl->gpu_data.specular = {0.3f, 0.3f, 0.3f};
    pl->gpu_data.radius = 20.0f;
    pl->gpu_data.type = KGPU_light_type_point;
    pl->gpu_data.cut_off = -1.0f;
    pl->gpu_data.outer_cut_off = -1.0f;
    renderer.schd_add_light(pl);

    // Fracture a unit cube (vertices + indices identical to create_cube_mesh)
    // clang-format off
    std::vector<gpu::vertex_data> cube_v = {
        {{-0.5f,-0.5f, 0.5f},{0,0, 1},{1,1,1},{0,0}},
        {{ 0.5f,-0.5f, 0.5f},{0,0, 1},{1,1,1},{1,0}},
        {{ 0.5f, 0.5f, 0.5f},{0,0, 1},{1,1,1},{1,1}},
        {{-0.5f, 0.5f, 0.5f},{0,0, 1},{1,1,1},{0,1}},
        {{ 0.5f,-0.5f,-0.5f},{0,0,-1},{1,1,1},{0,0}},
        {{-0.5f,-0.5f,-0.5f},{0,0,-1},{1,1,1},{1,0}},
        {{-0.5f, 0.5f,-0.5f},{0,0,-1},{1,1,1},{1,1}},
        {{ 0.5f, 0.5f,-0.5f},{0,0,-1},{1,1,1},{0,1}},
        {{-0.5f, 0.5f, 0.5f},{0,1,0},{1,1,1},{0,0}},
        {{ 0.5f, 0.5f, 0.5f},{0,1,0},{1,1,1},{1,0}},
        {{ 0.5f, 0.5f,-0.5f},{0,1,0},{1,1,1},{1,1}},
        {{-0.5f, 0.5f,-0.5f},{0,1,0},{1,1,1},{0,1}},
        {{-0.5f,-0.5f,-0.5f},{0,-1,0},{1,1,1},{0,0}},
        {{ 0.5f,-0.5f,-0.5f},{0,-1,0},{1,1,1},{1,0}},
        {{ 0.5f,-0.5f, 0.5f},{0,-1,0},{1,1,1},{1,1}},
        {{-0.5f,-0.5f, 0.5f},{0,-1,0},{1,1,1},{0,1}},
        {{ 0.5f,-0.5f, 0.5f},{1,0,0},{1,1,1},{0,0}},
        {{ 0.5f,-0.5f,-0.5f},{1,0,0},{1,1,1},{1,0}},
        {{ 0.5f, 0.5f,-0.5f},{1,0,0},{1,1,1},{1,1}},
        {{ 0.5f, 0.5f, 0.5f},{1,0,0},{1,1,1},{0,1}},
        {{-0.5f,-0.5f,-0.5f},{-1,0,0},{1,1,1},{0,0}},
        {{-0.5f,-0.5f, 0.5f},{-1,0,0},{1,1,1},{1,0}},
        {{-0.5f, 0.5f, 0.5f},{-1,0,0},{1,1,1},{1,1}},
        {{-0.5f, 0.5f,-0.5f},{-1,0,0},{1,1,1},{0,1}},
    };
    std::vector<gpu::uint> cube_i = {
         0, 1, 2,  2, 3, 0,
         4, 5, 6,  6, 7, 4,
         8, 9,10, 10,11, 8,
        12,13,14, 14,15,12,
        16,17,18, 18,19,16,
        20,21,22, 22,23,20,
    };
    // clang-format on

    voronoi_fracture::fracture_params fp;
    fp.seed = 42;
    fp.cell_count = 6;
    fp.fill = voronoi_fracture::fill_mode::convex;
    fp.roughness = 0.0f;
    auto result = voronoi_fracture::fracture_mesh(
        cube_v.data(), (uint32_t)cube_v.size(), cube_i.data(), (uint32_t)cube_i.size(), fp);
    ASSERT_FALSE(result.chunks.empty());

    // Diagnostic: check normals for each chunk.
    // Positions are already relative to seed_point (origin), so use origin as center.
    uint32_t total_tris = 0;
    uint32_t inward_tris = 0;
    for (uint32_t ci = 0; ci < result.chunks.size(); ++ci)
    {
        auto& ck = result.chunks[ci];
        glm::vec3 center{0.0f};

        for (uint32_t t = 0; t + 2 < ck.indices.size(); t += 3)
        {
            auto& v0 = ck.vertices[ck.indices[t]];
            glm::vec3 mid =
                (ck.vertices[ck.indices[t]].position + ck.vertices[ck.indices[t + 1]].position +
                 ck.vertices[ck.indices[t + 2]].position) /
                3.0f;
            bool faces_outward = glm::dot(v0.normal, mid - center) >= 0.0f;
            ++total_tris;
            if (!faces_outward)
            {
                ++inward_tris;
            }
        }
    }
    float inward_pct = total_tris > 0 ? 100.0f * inward_tris / total_tris : 0.0f;
    printf("\n[chunk_normals] total triangles: %u, inward-facing: %u (%.1f%%)\n",
           total_tris,
           inward_tris,
           inward_pct);

    // Verify vertex_data layout matches GPU expectations
    printf(
        "[chunk_normals] sizeof(vertex_data)=%zu  "
        "offset: pos=%zu normal=%zu color=%zu uv=%zu uv2=%zu\n",
        sizeof(gpu::vertex_data),
        offsetof(gpu::vertex_data, position),
        offsetof(gpu::vertex_data, normal),
        offsetof(gpu::vertex_data, color),
        offsetof(gpu::vertex_data, uv),
        offsetof(gpu::vertex_data, uv2));

    // Print first chunk's first few triangle normals + raw bytes at normal offset
    if (!result.chunks.empty())
    {
        auto& ck = result.chunks[0];
        uint32_t n = std::min(uint32_t(ck.indices.size() / 3), 4u);
        for (uint32_t t = 0; t < n; ++t)
        {
            auto& v = ck.vertices[ck.indices[t * 3]];
            auto* raw = reinterpret_cast<const float*>(&v);
            printf(
                "[chunk_normals] tri %u: normal=(%.3f,%.3f,%.3f)  "
                "raw floats[0..7]=(%.3f,%.3f,%.3f, %.3f,%.3f,%.3f, %.3f,%.3f)\n",
                t,
                v.normal[0],
                v.normal[1],
                v.normal[2],
                raw[0],
                raw[1],
                raw[2],
                raw[3],
                raw[4],
                raw[5],
                raw[6],
                raw[7]);
        }
    }

    // All normals of a convex chunk must face outward from centroid
    EXPECT_EQ(inward_tris, 0u) << inward_tris << "/" << total_tris
                               << " triangles have inward normals";

    // Verify each chunk has unique, non-degenerate triangles
    if (!result.chunks.empty())
    {
        auto& ck = result.chunks[0];
        printf("[chunk_normals] chunk 0: %zu verts, %zu indices\n",
               ck.vertices.size(),
               ck.indices.size());
        // Print vertex 0 raw bytes
        auto* raw = reinterpret_cast<const uint8_t*>(&ck.vertices[0]);
        printf("[chunk_normals] vertex 0 hex (52 bytes):");
        for (int b = 0; b < 52; ++b)
        {
            printf(" %02x", raw[b]);
        }
        printf("\n");
    }

    // Place each chunk separated on a floor, under the point light.
    // Also add a control quad with known upward normal to verify the pipeline.
    std::vector<texture_sampler_data> no_tex;
    auto grey_gpu = make_solid_color_gpu_data(
        {0.15f, 0.15f, 0.15f}, {0.7f, 0.7f, 0.7f}, {0.3f, 0.3f, 0.3f}, 32.0f);

    {
        // Control: a simple quad built with the same buffer→create_mesh path as chunks
        std::vector<gpu::vertex_data> ctrl_v = {
            {{-0.4f, 0, 0.4f}, {0, 1, 0}, {1, 1, 1}, {0, 0}},
            {{0.4f, 0, 0.4f}, {0, 1, 0}, {1, 1, 1}, {1, 0}},
            {{0.4f, 0, -0.4f}, {0, 1, 0}, {1, 1, 1}, {1, 1}},
            {{-0.4f, 0, -0.4f}, {0, 1, 0}, {1, 1, 1}, {0, 1}},
        };
        std::vector<gpu::uint> ctrl_i = {0, 1, 2, 2, 3, 0};

        kryga::utils::buffer cvb(ctrl_v.size() * sizeof(gpu::vertex_data));
        std::memcpy(cvb.data(), ctrl_v.data(), cvb.size());
        kryga::utils::buffer cib(ctrl_i.size() * sizeof(gpu::uint));
        std::memcpy(cib.data(), ctrl_i.data(), cib.size());

        auto* ctrl_mat = loader.create_material(
            AID("cnpl_ctrl_m"), AID("solid_color_material"), no_tex, *se, grey_gpu);
        renderer.schd_add_material(ctrl_mat);

        auto* ctrl_mesh = loader.create_mesh(
            AID("cnpl_ctrl_mesh"), cvb.make_view<gpu::vertex_data>(), cib.make_view<gpu::uint>());

        glm::vec3 ctrl_pos{0.0f, 0.0f, 2.0f};
        auto ctrl_model = glm::translate(glm::mat4(1.0f), ctrl_pos);
        auto* ctrl_obj = cache.objects.alloc(AID("cnpl_ctrl"));
        loader.update_object(*ctrl_obj,
                             *ctrl_mat,
                             *ctrl_mesh,
                             ctrl_model,
                             glm::transpose(glm::inverse(ctrl_model)),
                             ctrl_pos);
        renderer.schd_add_object(ctrl_obj);
    }

    for (uint32_t i = 0; i < result.chunks.size(); ++i)
    {
        auto& ck = result.chunks[i];
        if (ck.indices.empty())
        {
            continue;
        }

        auto tag = std::to_string(i);
        auto* mat = loader.create_material(
            AID(("cnpl_m_" + tag).c_str()), AID("solid_color_material"), no_tex, *se, grey_gpu);
        renderer.schd_add_material(mat);

        kryga::utils::buffer vb(ck.vertices.size() * sizeof(gpu::vertex_data));
        std::memcpy(vb.data(), ck.vertices.data(), vb.size());
        kryga::utils::buffer ib(ck.indices.size() * sizeof(gpu::uint));
        std::memcpy(ib.data(), ck.indices.data(), ib.size());

        auto* mesh = loader.create_mesh(AID(("cnpl_mesh_" + tag).c_str()),
                                        vb.make_view<gpu::vertex_data>(),
                                        ib.make_view<gpu::uint>());

        // Spread chunks in a row
        float x_offset = (float(i) - float(result.chunks.size()) / 2.0f) * 1.5f;
        glm::vec3 pos{x_offset, 0.5f, 0.0f};
        auto model = glm::translate(glm::mat4(1.0f), pos);

        auto* obj = cache.objects.alloc(AID(("cnpl_o_" + tag).c_str()));
        loader.update_object(*obj, *mat, *mesh, model, glm::transpose(glm::inverse(model)), pos);
        renderer.schd_add_object(obj);
    }

    renderer.draw_headless();
    compare("chunk_normals_point_light", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

TEST_F(visual_pipeline_test, voronoi_fractured_convex)
{
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
    auto& cache = renderer.get_cache();

    auto* se = create_lit_shader_effect(AID("se_frac_cx"));
    ASSERT_TRUE(se);

    auto scene = setup_scene("frac_cx", {3, 2.5f, 4}, {0, 0, 0}, se);

    std::vector<gpu::vertex_data> sphere_verts;
    std::vector<gpu::uint> sphere_idx;
    {
        constexpr uint32_t stacks = 32, slices = 32;
        constexpr float radius = 1.0f;
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
                sphere_verts.push_back({p, n, {1, 1, 1}, uv});
            }
        }
        for (uint32_t i = 0; i < stacks; ++i)
        {
            for (uint32_t j = 0; j < slices; ++j)
            {
                uint32_t a = i * (slices + 1) + j;
                uint32_t b = a + slices + 1;
                sphere_idx.insert(sphere_idx.end(), {a, a + 1, b, a + 1, b + 1, b});
            }
        }
    }

    voronoi_fracture::fracture_params fp;
    fp.seed = 42;
    fp.cell_count = 8;
    fp.fill = voronoi_fracture::fill_mode::convex;
    fp.roughness = 0.0f;
    fp.depth = 2;
    auto result = voronoi_fracture::fracture_mesh(sphere_verts.data(),
                                                  (uint32_t)sphere_verts.size(),
                                                  sphere_idx.data(),
                                                  (uint32_t)sphere_idx.size(),
                                                  fp);
    ASSERT_FALSE(result.chunks.empty());

    // clang-format off
    const glm::vec3 palette[] = {
        {0.9f, 0.2f, 0.2f}, {0.2f, 0.7f, 0.3f}, {0.2f, 0.3f, 0.9f},
        {0.9f, 0.8f, 0.1f}, {0.8f, 0.3f, 0.8f}, {0.1f, 0.8f, 0.8f},
        {0.9f, 0.5f, 0.1f}, {0.4f, 0.9f, 0.7f},
    };
    // clang-format on

    std::vector<texture_sampler_data> no_tex;

    for (uint32_t i = 0; i < result.chunks.size(); ++i)
    {
        auto& ck = result.chunks[i];
        if (ck.indices.empty())
        {
            continue;
        }

        auto suffix = std::to_string(i);
        auto color = palette[i % 8];

        auto gpu_data = make_solid_color_gpu_data(color * 0.4f, color, {0.6f, 0.6f, 0.6f}, 32.0f);
        auto* mat = loader.create_material(
            AID(("fcx_mat_" + suffix).c_str()), AID("solid_color_material"), no_tex, *se, gpu_data);
        renderer.schd_add_material(mat);

        kryga::utils::buffer vb(ck.vertices.size() * sizeof(gpu::vertex_data));
        std::memcpy(vb.data(), ck.vertices.data(), vb.size());
        kryga::utils::buffer ib(ck.indices.size() * sizeof(gpu::uint));
        std::memcpy(ib.data(), ck.indices.data(), ib.size());

        auto* mesh = loader.create_mesh(AID(("fcx_mesh_" + suffix).c_str()),
                                        vb.make_view<gpu::vertex_data>(),
                                        ib.make_view<gpu::uint>());

        glm::vec3 center = (ck.aabb_min + ck.aabb_max) * 0.5f;
        float len = glm::length(center);
        glm::vec3 dir = len > 1e-6f ? center / len : glm::vec3(0, 1, 0);
        glm::vec3 offset = dir * 0.6f;
        auto model = glm::translate(glm::mat4(1.0f), offset);

        auto* obj = cache.objects.alloc(AID(("fcx_obj_" + suffix).c_str()));
        loader.update_object(*obj, *mat, *mesh, model, glm::transpose(glm::inverse(model)), offset);
        renderer.schd_add_object(obj);
    }

    renderer.draw_headless();
    compare("voronoi_fractured_convex", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

TEST_F(visual_pipeline_test, voronoi_presets)
{
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& loader = glob::glob_state().getr_render().loader;
    auto& cache = renderer.get_cache();

    auto* se = create_lit_shader_effect(AID("se_vp"));
    ASSERT_TRUE(se);

    auto scene = setup_scene("vp", {0, 3.0f, 10}, {0, 0, 0}, se);

    // Shared sphere
    std::vector<gpu::vertex_data> sv;
    std::vector<gpu::uint> si;
    {
        constexpr uint32_t stacks = 32, slices = 32;
        for (uint32_t i = 0; i <= stacks; ++i)
        {
            float phi = glm::pi<float>() * float(i) / float(stacks);
            for (uint32_t j = 0; j <= slices; ++j)
            {
                float theta = 2.0f * glm::pi<float>() * float(j) / float(slices);
                glm::vec3 n = {std::sin(phi) * std::cos(theta),
                               std::cos(phi),
                               std::sin(phi) * std::sin(theta)};
                sv.push_back({n, n, {1, 1, 1}, {float(j) / slices, float(i) / stacks}});
            }
        }
        for (uint32_t i = 0; i < stacks; ++i)
        {
            for (uint32_t j = 0; j < slices; ++j)
            {
                uint32_t a = i * (slices + 1) + j, b = a + slices + 1;
                si.insert(si.end(), {a, a + 1, b, a + 1, b + 1, b});
            }
        }
    }

    // clang-format off
    const glm::vec3 palette[] = {
        {0.9f, 0.2f, 0.2f}, {0.2f, 0.7f, 0.3f}, {0.2f, 0.3f, 0.9f},
        {0.9f, 0.8f, 0.1f}, {0.8f, 0.3f, 0.8f}, {0.1f, 0.8f, 0.8f},
        {0.9f, 0.5f, 0.1f}, {0.4f, 0.9f, 0.7f},
    };
    // clang-format on

    struct preset
    {
        float x;
        voronoi_fracture::fracture_params fp;
    };

    // 1=baseline  2=detail×4  3=depth×2  4=rough  5=all combined
    preset presets[] = {
        {-4.0f, {42, 8, voronoi_fracture::fill_mode::convex, 0.0f, 1, 1}},
        {-2.0f, {42, 8, voronoi_fracture::fill_mode::convex, 0.0f, 1, 4}},
        {0.0f, {42, 8, voronoi_fracture::fill_mode::convex, 0.0f, 2, 1}},
        {2.0f, {42, 8, voronoi_fracture::fill_mode::convex, 0.05f, 1, 1}},
        {4.0f, {42, 8, voronoi_fracture::fill_mode::convex, 0.02f, 2, 4}},
    };

    std::vector<texture_sampler_data> no_tex;

    for (int pi = 0; pi < 5; ++pi)
    {
        auto& pr = presets[pi];
        auto result = voronoi_fracture::fracture_mesh(
            sv.data(), (uint32_t)sv.size(), si.data(), (uint32_t)si.size(), pr.fp);

        for (uint32_t i = 0; i < result.chunks.size(); ++i)
        {
            auto& ck = result.chunks[i];
            if (ck.indices.empty())
            {
                continue;
            }

            auto tag = std::to_string(pi) + "_" + std::to_string(i);
            auto color = palette[i % 8];

            auto gpu_data =
                make_solid_color_gpu_data(color * 0.4f, color, {0.6f, 0.6f, 0.6f}, 32.0f);
            auto* mat = loader.create_material(
                AID(("vp_m_" + tag).c_str()), AID("solid_color_material"), no_tex, *se, gpu_data);
            renderer.schd_add_material(mat);

            kryga::utils::buffer vb(ck.vertices.size() * sizeof(gpu::vertex_data));
            std::memcpy(vb.data(), ck.vertices.data(), vb.size());
            kryga::utils::buffer ib(ck.indices.size() * sizeof(gpu::uint));
            std::memcpy(ib.data(), ck.indices.data(), ib.size());

            auto* mesh = loader.create_mesh(AID(("vp_x_" + tag).c_str()),
                                            vb.make_view<gpu::vertex_data>(),
                                            ib.make_view<gpu::uint>());

            glm::vec3 center = (ck.aabb_min + ck.aabb_max) * 0.5f;
            float len = glm::length(center);
            glm::vec3 dir = len > 1e-6f ? center / len : glm::vec3(0, 1, 0);
            glm::vec3 offset = glm::vec3(pr.x, 0, 0) + dir * 0.3f;
            auto model = glm::translate(glm::mat4(1.0f), offset);

            auto* obj = cache.objects.alloc(AID(("vp_o_" + tag).c_str()));
            loader.update_object(
                *obj, *mat, *mesh, model, glm::transpose(glm::inverse(model)), offset);
            renderer.schd_add_object(obj);
        }
    }

    renderer.draw_headless();
    compare("voronoi_presets", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}

// =============================================================================
// No-op: grid-offscreen
// =============================================================================
TEST_F(visual_pipeline_test, noop_grid_offscreen)
{
    // Anti-regression: enable the grid at init, but point the camera at the
    // empty sky (away from XZ plane) so no grid pixel can be in frame. Output
    // must equal a plain empty_frame render — proves grid post doesn't bleed
    // into pixels it shouldn't.
    render_config cfg{};
    cfg.debug.show_grid = true;
    cfg.debug.light_wireframe = false;
    cfg.debug.light_icons = false;
    reinit_with_config(cfg);

    auto& renderer = glob::glob_state().getr_render().renderer;

    // Camera at origin looking straight up — XZ plane (grid) is at infinity
    // behind us, never in frame.
    gpu::camera_data cam;
    cam.projection =
        glm::perspective(glm::radians(60.0f), float(TEST_WIDTH) / float(TEST_HEIGHT), 0.1f, 256.0f);
    cam.projection[1][1] *= -1.0f;
    cam.view = glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1));
    cam.inv_projection = glm::inverse(cam.projection);
    cam.position = {0, 0, 0};
    renderer.set_camera(cam);

    renderer.draw_headless();
    compare("noop_grid_offscreen", *m_main_pass, TEST_WIDTH, TEST_HEIGHT);
}
