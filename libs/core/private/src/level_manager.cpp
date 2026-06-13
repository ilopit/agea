#include "core/level_manager.h"

#include "core/level.h"
#include "core/model_system.h"
#include "core/object_constructor.h"
#include "core/package_manager.h"
#include "core/model_output.h"
#include "core/caches/cache_set.h"
#include "core/caches/hash_cache.h"
#include "core/caches/caches_map.h"

#include <packages/root/model/components/game_object_component.h>
#include <packages/root/model/game_object.h>

#include <utils/kryga_log.h>

#include <vfs/vfs.h>

#include <serialization/serialization.h>

namespace kryga
{
namespace core
{

level*
level_manager::load_level(const utils::id& id)
{
    ALOG_INFO("Begin level loading with id {0}", id.cstr());

    auto& l = m_levels[id];

    if (l)
    {
        if (l->get_state() == level_state::loaded)
        {
            ALOG_INFO("[{0}] already loaded", id.cstr());
            return l.get();
        }
    }
    else
    {
        l = std::make_unique<level>(id);
    }

    auto vfs_root = vfs_paths::level_root(id);
    KRG_check(vfs_paths::is_valid_level_root(vfs_root), "Level must be under data://levels/");
    if (!glob::glob_state().getr_vfs().exists(vfs_root))
    {
        ALOG_ERROR("Level not found: {}", vfs_root.str());
        return nullptr;
    }
    l->set_vfs_root(vfs_root);

    return load_level_path(*l, vfs_root);
}

level*
level_manager::load_level_path(level& l, const vfs::rid& vfs_root)
{
    ALOG_INFO("Begin level loading at {0}", vfs_root.str());

    auto root_rid = vfs_root / "root.cfg";

    serialization::container container;
    if (!serialization::read_container(root_rid, container))
    {
        ALOG_LAZY_ERROR;
        return nullptr;
    }

    auto& vfs = glob::glob_state().getr_vfs();

    // Populate the file index from the cooked `.kryga_index` manifest emitted
    // by tools/cook. Works uniformly on desktop physical mounts and Android
    // APK asset mounts — no `real_path` required.
    l.m_backend = vfs.mount_from_manifest(vfs_root,
                                          vfs_root / "kryga_index",
                                          {.load_order = {"class/textures",
                                                          "class/shader_effects",
                                                          "class/materials",
                                                          "class/meshes",
                                                          "class/components"}});

    // Fallback: walk the filesystem if no manifest. Desktop-only — Android
    // APK assets have no real_path so the cooked manifest is required there.
    // This lets uncooked content (e.g. converter output in regression tests)
    // load without a separate cook step.
    if (!l.m_backend)
    {
        auto rp = vfs.real_path(vfs_root);
        if (rp.has_value())
        {
            ALOG_WARN(
                "Level [{}] has no kryga_index manifest; falling back to filesystem "
                "scan at [{}]. This path will NOT work in GAME mode (cooked/shipping "
                "builds, Android APK) — run tools/cook before shipping.",
                vfs_root.str(),
                rp.value().string());
            l.m_backend = vfs.mount(vfs_root,
                                    rp.value(),
                                    {.index_filter = ".aobj",
                                     .load_order = {"class/textures",
                                                    "class/shader_effects",
                                                    "class/materials",
                                                    "class/meshes",
                                                    "class/components"}});
        }
    }
    if (!l.m_backend)
    {
        ALOG_LAZY_ERROR;
        return nullptr;
    }

    l.m_occ->set_vfs_mount(vfs_root);

    {
        auto packages = container["packages"];
        auto packages_count = packages.size();
        for (size_t idx = 0; idx < packages_count; ++idx)
        {
            auto pkg_str = packages[idx].as<std::string>();
            // Strip .apkg extension if present — id is the bare name
            if (pkg_str.size() > 5 && pkg_str.substr(pkg_str.size() - 5) == ".apkg")
            {
                pkg_str.resize(pkg_str.size() - 5);
            }
            auto id = AID(pkg_str);
            if (!glob::glob_state().getr_model().packages.load_package(id))
            {
                ALOG_LAZY_ERROR;
                return nullptr;
            }
            l.m_package_ids.push_back(id);
        }
    }

    {
        bool load_ok = true;
        object_constructor ctor(l.m_occ.get(), object_load_type::instance_obj);
        vfs.enumerate_objects(
            vfs_root,
            [&](std::string_view name, const vfs::rid&) -> bool
            {
                auto result = ctor.load_obj(AID(std::string(name)));
                if (!result)
                {
                    load_ok = false;
                    return false;
                }
                return true;
            },
            l.m_backend);

        if (!load_ok)
        {
            return nullptr;
        }

        auto loaded = l.m_occ->reset_loaded_objects();
        for (auto& i : loaded)
        {
            if (auto go = i->as<root::game_object>())
            {
                glob::glob_state().getr_model().output.dirty_render.emplace_back(
                    go->get_root_component());
            }
        }
    }
    // Read lightmap references from root.cfg (if present)
    if (container["lightmap_bin"])
    {
        auto bin_path = container["lightmap_bin"].as<std::string>("");
        auto manifest_path = container["lightmap_manifest"].as<std::string>("");
        if (!bin_path.empty())
        {
            l.m_lightmap_bin_rid = vfs_root / bin_path;
            l.m_lightmap_manifest_rid = vfs_root / manifest_path;
            ALOG_INFO("level_manager: level references lightmap at {}", bin_path);
        }
    }

    l.m_state = level_state::loaded;

    return &l;
}

void
level_manager::unload_level(level& l)
{
    glob::glob_state().getr_model().output.drop_pending();
    l.unregister_objects();
    l.unload();
}

bool
level_manager::save_level(level& l, const utils::path& path)
{
    l.set_vfs_root(vfs::rid("data", "levels/" + l.get_id().str() + ".alvl"));
    std::string name = l.get_id().str() + ".alvl";
    auto full_path = path / name;

    if (!full_path.exists())
    {
        std::filesystem::create_directories(full_path.fs());
    }

    serialization::container container;
    for (auto& id : l.m_package_ids)
    {
        container["packages"].push_back(id.str());
    }

    // Save lightmap references (relative to level root)
    if (l.has_lightmap_ref())
    {
        container["lightmap_bin"] = std::string(l.m_lightmap_bin_rid.relative());
        container["lightmap_manifest"] = std::string(l.m_lightmap_manifest_rid.relative());
    }

    auto root_path = full_path / "root.cfg";
    if (!serialization::write_container(root_path, container))
    {
        return false;
    }

    return true;
}

}  // namespace core
}  // namespace kryga
