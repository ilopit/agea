#include <engine/kryga_engine.h>

#include <global_state/global_state.h>
#include <utils/kryga_log.h>
#include <vfs/vfs.h>
#include <vfs/vfs_state.h>
#include <vfs/physical_backend.h>

#include <project_paths/project_paths.h>

#if defined(__ANDROID__)

#include <vfs/android_asset_backend.h>

#include <SDL.h>
#include <SDL_hints.h>
#include <SDL_system.h>

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include <jni.h>

#elif WIN32

#include <windows.h>
#include <shellscalingapi.h>
#pragma comment(lib, "Shcore.lib")

#endif

int
main(int argc, char** argv)
{
    kryga::utils::setup_logger(spdlog::level::level_enum::trace);

    // Hand argv[0] to the path resolver before anything else needs paths.
    // On Android argv[0] is the package name, not a path — pass nullptr so
    // resolver short-circuits to staged-only.
#if defined(__ANDROID__)
    kryga::paths::set_exe_path(nullptr);
#else
    kryga::paths::set_exe_path(argc > 0 ? argv[0] : nullptr);
#endif

    // Parse command-line arguments
    kryga::startup_options options;
    if (!kryga::startup_options::parse(argc, argv, options))
    {
        if (options.show_help)
        {
            kryga::startup_options::print_help(argv[0]);
            return 0;
        }
        return 1;
    }

#if WIN32
    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
#endif

#if defined(__ANDROID__)
    // AndroidManifest.xml declares `screenOrientation="landscape"` (fixed to
    // one landscape rotation). SDL derives its own `setRequestedOrientation`
    // call from this hint; when unset it defaults to SENSOR_LANDSCAPE (enum 6)
    // which lets the device flip between both landscape rotations. Match the
    // manifest so both layers agree and startup is predictable.
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft");
#endif
    {
        auto& gs = kryga::glob::glob_state();

        kryga::state_mutator__vfs::set(gs);
        auto& vfs = gs.getr_vfs();

#if defined(__ANDROID__)
        // Read-only APK assets for game data, shaders, levels, etc.
        JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
        jobject activity = (jobject)SDL_AndroidGetActivity();

        jclass activity_class = env->GetObjectClass(activity);
        jmethodID get_assets =
            env->GetMethodID(activity_class, "getAssets", "()Landroid/content/res/AssetManager;");
        jobject asset_manager_obj = env->CallObjectMethod(activity, get_assets);
        AAssetManager* asset_manager = AAssetManager_fromJava(env, asset_manager_obj);

        env->DeleteLocalRef(activity_class);
        env->DeleteLocalRef(activity);

        vfs.mount("data", std::make_unique<kryga::vfs::android_asset_backend>(asset_manager), 0);

        // Internal storage for cache/temp — physical, writable, sandboxed per-app.
        std::filesystem::path internal = SDL_AndroidGetInternalStoragePath();
        vfs.mount("cache", std::make_unique<kryga::vfs::physical_backend>(internal / "cache"), 0);
        vfs.mount(
            "rtcache", std::make_unique<kryga::vfs::physical_backend>(internal / "rtcache"), 0);
        vfs.mount("tmp", std::make_unique<kryga::vfs::physical_backend>(internal / "tmp"), 0);
        // `generated` points at argen.py output bundled into the APK under assets/kryga_generated/
        vfs.mount(
            "generated",
            std::make_unique<kryga::vfs::android_asset_backend>(asset_manager, "kryga_generated"),
            0);
#else
        auto layout = kryga::paths::resolve();
        if (!layout)
        {
            ALOG_ERROR("Failed to resolve project paths — engine cannot start");
            return 1;
        }
        const auto& root = layout->staged_root;

        // data:// — content root.
        //   editor: source resources/ so authored levels/shaders/etc. are
        //           visible without a cook step. Runtime GLSL compilation
        //           (vulkan_shader_loader.cpp:53) handles the .vert/.frag
        //           direct path when is_*_binary is false.
        //   game:   cooked output under staged_root.
#if KRG_EDITOR
        if (layout->source_root.empty())
        {
            ALOG_ERROR(
                "editor build requires a dev layout (source_root) — "
                "no 'kryga.project' anchor found");
            return 1;
        }
        vfs.mount("data",
                  std::make_unique<kryga::vfs::physical_backend>(layout->source_root / "resources"),
                  0);
        // Shaders write `#include "gpu_types/foo.h"`. The cooker stages a
        // gpu_types symlink at staged_root pointing at this dir; the editor
        // bypasses staging so we mount it explicitly at lower priority.
        vfs.mount("data",
                  std::make_unique<kryga::vfs::physical_backend>(
                      layout->source_root / "libs/render/gpu_types/public/include"),
                  -1);
#else
        vfs.mount("data", std::make_unique<kryga::vfs::physical_backend>(root), 0);
#endif
        // When --discovery is set, isolate mutable VFS mounts so multiple
        // engine instances don't clobber each other's shader cache / tmp files.
        auto instance_id = std::filesystem::path(options.discovery).stem().string();
        auto suffix = instance_id.empty() ? "" : ("_" + instance_id);

        vfs.mount(
            "cache", std::make_unique<kryga::vfs::physical_backend>(root / ("cache" + suffix)), 0);
        vfs.mount("rtcache",
                  std::make_unique<kryga::vfs::physical_backend>(root / ("rtcache" + suffix)),
                  0);
        vfs.mount(
            "tmp", std::make_unique<kryga::vfs::physical_backend>(root / ("tmp" + suffix)), 0);
        vfs.mount(
            "generated", std::make_unique<kryga::vfs::physical_backend>(layout->generated_dir), 0);
#endif

        kryga::vulkan_engine engine;
        kryga::state_mutator__engine::set(&engine, gs);

        engine.init(options);
        engine.run();
        engine.cleanup();
    }

    return 0;
}
