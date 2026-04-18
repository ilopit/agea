#include <engine/kryga_engine.h>

#include <global_state/global_state.h>
#include <utils/kryga_log.h>
#include <vfs/vfs.h>
#include <vfs/vfs_state.h>
#include <vfs/physical_backend.h>

#if defined(__ANDROID__)

#include <vfs/android_asset_backend.h>

#include <SDL.h>
#include <SDL_system.h>

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include <jni.h>

#elif WIN32

#include <windows.h>
#include <shellscalingapi.h>
#pragma comment(lib, "Shcore.lib")

#endif

#if !defined(__ANDROID__)
static std::filesystem::path
get_exe_dir()
{
#if WIN32
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return std::filesystem::path(buf).parent_path();
#else
    return std::filesystem::canonical("/proc/self/exe").parent_path();
#endif
}
#endif

int
main(int argc, char** argv)
{
    kryga::utils::setup_logger(spdlog::level::level_enum::trace);

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
    {
        auto& gs = kryga::glob::glob_state();

        kryga::state_mutator__vfs::set(gs);
        auto& vfs = gs.getr_vfs();

#if defined(__ANDROID__)
        // Read-only APK assets for game data, shaders, levels, etc.
        JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
        jobject activity = (jobject)SDL_AndroidGetActivity();

        jclass activity_class = env->GetObjectClass(activity);
        jmethodID get_assets = env->GetMethodID(activity_class, "getAssets", "()Landroid/content/res/AssetManager;");
        jobject asset_manager_obj = env->CallObjectMethod(activity, get_assets);
        AAssetManager* asset_manager = AAssetManager_fromJava(env, asset_manager_obj);

        env->DeleteLocalRef(activity_class);
        env->DeleteLocalRef(activity);

        vfs.mount("data", std::make_unique<kryga::vfs::android_asset_backend>(asset_manager), 0);

        // Internal storage for cache/temp — physical, writable, sandboxed per-app.
        std::filesystem::path internal = SDL_AndroidGetInternalStoragePath();
        vfs.mount("cache",     std::make_unique<kryga::vfs::physical_backend>(internal / "cache"), 0);
        vfs.mount("rtcache",   std::make_unique<kryga::vfs::physical_backend>(internal / "rtcache"), 0);
        vfs.mount("tmp",       std::make_unique<kryga::vfs::physical_backend>(internal / "tmp"), 0);
        // `generated` points at argen.py output bundled into the APK under assets/kryga_generated/
        vfs.mount("generated", std::make_unique<kryga::vfs::android_asset_backend>(asset_manager, "kryga_generated"), 0);
#else
        auto root = get_exe_dir().parent_path();
        vfs.mount("data", std::make_unique<kryga::vfs::physical_backend>(root), 0);
        vfs.mount("cache", std::make_unique<kryga::vfs::physical_backend>(root / "cache"), 0);
        vfs.mount("rtcache", std::make_unique<kryga::vfs::physical_backend>(root / "rtcache"), 0);
        vfs.mount("tmp", std::make_unique<kryga::vfs::physical_backend>(root / "tmp"), 0);
        vfs.mount(
            "generated",
            std::make_unique<kryga::vfs::physical_backend>(root.parent_path() / "kryga_generated"),
            0);
#endif

        kryga::vulkan_engine engine;
        kryga::state_mutator__engine::set(&engine, gs);

        engine.init(options);
        engine.run();
        engine.cleanup();
    }

    return 0;
}
