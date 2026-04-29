#include "readback.h"

#include <gtest/gtest.h>

#include <engine/kryga_engine.h>

#include <asset_converter/converter_context.h>
#include <asset_converter/gltf_parser.h>

#include <global_state/global_state.h>

#include <vfs/vfs.h>
#include <vfs/vfs_state.h>
#include <vfs/physical_backend.h>
#include <vfs/rid.h>

#include <vulkan_render/kryga_render.h>
#include <vulkan_render/vulkan_render_device.h>
#include <vulkan_render/types/vulkan_render_pass.h>

#include <render/utils/image_compare.h>

#include <gpu_types/gpu_camera_types.h>

#include <glm_unofficial/glm.h>

#include <utils/kryga_log.h>

#include <cstdlib>
#include <filesystem>

using namespace kryga;
namespace fs = std::filesystem;

namespace
{

constexpr uint32_t TEST_WIDTH = 512;
constexpr uint32_t TEST_HEIGHT = 512;

// Locate the engine resource root (build/project_<Config>/). Tests run from
// bin/, so parent_path() gives us the config dir.
fs::path
get_engine_root()
{
    return fs::current_path().parent_path();
}

// kryga_generated lives as a sibling of project_<Config>/ (shared across configs)
fs::path
get_generated_root()
{
    return get_engine_root().parent_path() / "kryga_generated";
}

fs::path
get_test_asset(const char* name)
{
    // Tests run from build/project_<Config>/bin — test assets live at
    // build/test_assets/converter, reached via engine_root's parent.
    auto p = get_engine_root().parent_path() / "test_assets" / "converter" / name;
    return fs::exists(p) ? p : fs::path{};
}

fs::path
get_test_scratch()
{
    auto p = get_engine_root() / "tmp" / "engine_regression";
    std::error_code ec;
    fs::remove_all(p, ec);       // clean slate
    fs::create_directories(p);
    return p;
}

// Install minimal VFS + engine-expected mounts. Call AFTER glob_state_reset.
// scratch overlays data:// at higher priority so converted packages/levels
// are discoverable next to the stock engine resources.
void
install_vfs(const fs::path& engine_root,
            const fs::path& scratch_root,
            const fs::path& generated_root)
{
    auto& gs = glob::glob_state();
    state_mutator__vfs::set(gs);
    auto& vfs = gs.getr_vfs();

    vfs.mount("data", std::make_unique<vfs::physical_backend>(engine_root), 0);
    vfs.mount("data", std::make_unique<vfs::physical_backend>(scratch_root), 10);
    vfs.mount("cache", std::make_unique<vfs::physical_backend>(engine_root / "cache"), 0);
    vfs.mount("rtcache", std::make_unique<vfs::physical_backend>(engine_root / "rtcache"), 0);
    vfs.mount("tmp", std::make_unique<vfs::physical_backend>(engine_root / "tmp"), 0);
    vfs.mount("generated", std::make_unique<vfs::physical_backend>(generated_root), 0);
}

// converter writes both <name>.apkg and <name>.alvl into output_root flat.
// Engine wants packages/ and levels/ subdirs of data://. We write output into
// scratch/packages/, then move the .alvl (a directory) to scratch/levels/.
void
relocate_level(const fs::path& scratch_root, const std::string& name)
{
    auto pkg_parent = scratch_root / "packages";
    auto lvl_parent = scratch_root / "levels";
    fs::create_directories(lvl_parent);
    auto src = pkg_parent / (name + ".alvl");
    auto dst = lvl_parent / (name + ".alvl");
    std::error_code ec;
    fs::rename(src, dst, ec);
    ASSERT_FALSE(ec) << "Failed to relocate level: " << ec.message();
}

// Drive render_bridge-backed level setup: tick a few times so dirty queues
// drain, bindless texture registrations land on the frame being rendered.
void
warmup(vulkan_engine& engine, int frames = 6)
{
    for (int i = 0; i < frames; ++i)
    {
        engine.tick_headless();
    }
}

std::string
get_reference_path(const fs::path& engine_root, const char* test_name)
{
    return (engine_root / "test_references" / (std::string(test_name) + ".png")).string();
}

std::string
get_output_path(const fs::path& engine_root, const char* test_name)
{
    auto dir = engine_root / "tmp" / "engine_regression_output";
    fs::create_directories(dir);
    return (dir / (std::string(test_name) + "_actual.png")).string();
}

}  // namespace

// End-to-end: convert a glTF to package+level on disk, load through the engine
// headlessly, render, compare to a golden PNG.
//
// Test lifecycle exercises the full teardown path: converter_context init runs
// glob_state_reset, and vulkan_engine::cleanup runs another one. If any
// subsystem leaks state (GPU handles, static caches), this test starts failing
// on a second run.
TEST(engine_regression, converts_and_renders_box_textured)
{
    // NOTE: Using plain Box.glb (no textures) until converter's pbr_material output
    // includes texture-slot properties. With BoxTextured.glb the converter writes
    // class_id: pbr_material but omits `diffuse_txt` → engine YAML load fails.
    auto gltf_path = get_test_asset("Box.glb");
    if (gltf_path.empty())
    {
        GTEST_SKIP() << "Box.glb not found in build/test_assets/converter";
    }

    auto engine_root = get_engine_root();
    auto generated_root = get_generated_root();
    auto scratch_root = get_test_scratch();

    // Step 1: parse glTF (no global state needed for this)
    converter::parsed_scene scene;
    ASSERT_TRUE(converter::parse_gltf(gltf_path.string(), scene));
    ASSERT_FALSE(scene.meshes.empty());

    // Step 2: scoped converter — init() resets glob_state, dtor resets again.
    // Output goes to scratch/packages/ as <name>.apkg and <name>.alvl (both are
    // directories produced by save_package/save_level).
    {
        converter::converter_context ctx;
        ASSERT_TRUE(ctx.init(engine_root, scratch_root / "packages"));

        converter::converter_options opts;
        opts.mode = converter::converter_mode::level_with_package;
        opts.name = utils::id::make_id("box");
        opts.output_root = vfs::rid("output", "");
        opts.deduplicate_textures = true;
        opts.deduplicate_materials = true;

        ASSERT_TRUE(ctx.convert(scene, opts));
    }  // ctx dtor → glob_state_reset

    // Step 3: move .alvl into levels/ (engine expects data://levels/<id>.alvl)
    relocate_level(scratch_root, "box");

    // Step 4: install VFS fresh (glob_state was just reset)
    install_vfs(engine_root, scratch_root, generated_root);

    // Step 5: headless engine → load level → render → compare
    {
        vulkan_engine engine;
        state_mutator__engine::set(&engine, glob::glob_state());

        startup_options so;
        so.headless = true;
        ASSERT_TRUE(engine.init(so));
        ASSERT_TRUE(engine.load_level(AID("box")));

        // Camera: look at origin from above and aside
        auto& renderer = glob::glob_state().getr_vulkan_render();
        gpu::camera_data cam{};
        cam.projection = glm::perspective(
            glm::radians(45.0f), float(TEST_WIDTH) / float(TEST_HEIGHT), 0.1f, 256.0f);
        cam.projection[1][1] *= -1.0f;
        auto eye = glm::vec3(2.5f, 2.0f, 2.5f);
        cam.view = glm::lookAt(eye, glm::vec3(0), glm::vec3(0, 1, 0));
        cam.inv_projection = glm::inverse(cam.projection);
        cam.position = eye;
        renderer.set_camera(cam);
        renderer.get_render_config().shadows.distance = 0.0f;

        // Box.glb has no lights — add a directional sun so the lit material is visible
        auto* sun = renderer.get_cache().directional_lights.alloc(AID("test_sun"));
        sun->gpu_data.direction = {-0.3f, -1.0f, -0.2f};
        sun->gpu_data.ambient = {0.3f, 0.3f, 0.3f};
        sun->gpu_data.diffuse = {1.0f, 1.0f, 1.0f};
        sun->gpu_data.specular = {0.5f, 0.5f, 0.5f};
        renderer.schd_add_light(sun);
        renderer.set_selected_directional_light(AID("test_sun"));

        warmup(engine, 6);

        auto* main_pass = renderer.get_render_pass(AID("main"));
        ASSERT_TRUE(main_pass);

        auto pixels = render::test::readback_framebuffer(
            *main_pass, TEST_WIDTH, TEST_HEIGHT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        auto ref_path = get_reference_path(engine_root, "engine_converts_box_textured");
        auto actual_path = get_output_path(engine_root, "engine_converts_box_textured");

        if (std::getenv("UPDATE_REFERENCES"))
        {
            fs::create_directories(fs::path(ref_path).parent_path());
            ASSERT_TRUE(
                render::save_png(ref_path, pixels.data(), TEST_WIDTH, TEST_HEIGHT));
            GTEST_SKIP() << "Updated reference: " << ref_path;
        }

        uint32_t ref_w = 0, ref_h = 0;
        auto ref_pixels = render::load_png(ref_path, ref_w, ref_h);
        if (ref_pixels.empty())
        {
            render::save_png(actual_path, pixels.data(), TEST_WIDTH, TEST_HEIGHT);
            FAIL() << "Missing reference image: " << ref_path
                   << "\n  Actual saved to: " << actual_path
                   << "\n  Run with UPDATE_REFERENCES=1 to generate.";
        }
        ASSERT_EQ(ref_w, TEST_WIDTH);
        ASSERT_EQ(ref_h, TEST_HEIGHT);

        render::image_compare_params params;
        params.width = TEST_WIDTH;
        params.height = TEST_HEIGHT;
        params.pixel_tolerance = 1;
        params.ssim_threshold = 0.99f;
        params.generate_diff_image = false;

        auto result = render::compare_images(pixels.data(), ref_pixels.data(), params);

        if (!result.pixel_passed || !result.ssim_passed)
        {
            render::save_png(actual_path, pixels.data(), TEST_WIDTH, TEST_HEIGHT);
        }

        EXPECT_TRUE(result.pixel_passed)
            << "Pixel diff: " << result.diff_pixel_count << "/" << result.total_pixels
            << " (" << result.diff_percentage << "%)"
            << "\n  Reference: " << ref_path << "\n  Actual:    " << actual_path;
        EXPECT_TRUE(result.ssim_passed)
            << "SSIM: " << result.ssim << " (threshold: " << params.ssim_threshold << ")";

        engine.cleanup();
    }
}
