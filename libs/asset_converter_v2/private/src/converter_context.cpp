#include <asset_converter/converter_context.h>

#include <core/architype.h>
#include <core/caches/cache_set.h>
#include <core/core_state.h>
#include <core/model_system.h>
#include <core/level.h>
#include <core/level_manager.h>
#include <core/object_constructor.h>
#include <core/object_load_context.h>
#include <core/package.h>
#include <core/package_manager.h>
#include <core/model_output.h>

#include <global_state/global_state.h>

#include <packages/base/model/camera_object.h>
#include <packages/base/model/components/mesh_component.h>
#include <packages/base/model/components/camera_component.h>
#include <packages/base/model/lights/components/directional_light_component.h>
#include <packages/base/model/lights/components/point_light_component.h>
#include <packages/base/model/lights/components/spot_light_component.h>
#include <packages/base/model/lights/directional_light.h>
#include <packages/base/model/lights/point_light.h>
#include <packages/base/model/lights/spot_light.h>
#include <packages/base/model/assets/pbr_material.h>
#include <packages/base/model/mesh_object.h>
#include <packages/base/package.base.h>
#include <packages/base/package.base.types_builder.ar.h>
#include <packages/root/model/assets/material.h>
#include <packages/root/model/assets/mesh.h>
#include <packages/root/model/assets/texture.h>
#include <packages/root/model/assets/texture_slot.h>
#include <packages/root/model/components/game_object_component.h>
#include <packages/root/package.root.h>
#include <packages/root/package.root.types_builder.ar.h>

#include <serialization/serialization.h>

#include <vfs/physical_backend.h>
#include <vfs/vfs.h>
#include <vfs/vfs_state.h>

#include <utils/buffer.h>
#include <utils/kryga_log.h>

#include <fstream>

namespace kryga::converter
{

namespace
{

// vfs::rid has no parent() — strip the last segment of the relative part.
vfs::rid
parent_of(const vfs::rid& r)
{
    auto rel = r.relative();
    auto slash = rel.find_last_of('/');
    if (slash == std::string_view::npos)
    {
        return vfs::rid(r.mount_point(), "");
    }
    return vfs::rid(r.mount_point(), rel.substr(0, slash));
}

// Dump a package's proto caches for debugging lookups.
void
log_package_contents(core::package& pkg, const char* tag)
{
    auto& cs = pkg.get_local_cache();
    ALOG_INFO(
        "[converter] package [{}] contents ({}): {} meshes, {} textures, {} materials, {} "
        "shader_effects",
        pkg.get_id().str(),
        tag,
        cs.meshes.get_size(),
        cs.textures.get_size(),
        cs.materials.get_size(),
        cs.shader_effects.get_size());
    for (const auto& [id, obj] : cs.meshes.get_items())
    {
        ALOG_INFO("[converter]   mesh   [{}]", id.str());
    }
    for (const auto& [id, obj] : cs.textures.get_items())
    {
        ALOG_INFO("[converter]   tex    [{}]", id.str());
    }
    for (const auto& [id, obj] : cs.materials.get_items())
    {
        ALOG_INFO("[converter]   mat    [{}]", id.str());
    }
}

// Map an architype to its conventional subdirectory inside a package
// (matches package::init() load_order: class/textures, class/shader_effects,
// class/materials, class/meshes, class/components).
std::string
architype_subdir(core::architype a)
{
    switch (a)
    {
    case core::architype::mesh:
        return "class/meshes";
    case core::architype::texture:
        return "class/textures";
    case core::architype::material:
        return "class/materials";
    case core::architype::shader_effect:
        return "class/shader_effects";
    case core::architype::sampler:
        return "class/samplers";
    case core::architype::component:
        return "class/components";
    default:
        return {};
    }
}

}  // namespace

converter_context::converter_context() = default;

converter_context::~converter_context()
{
    if (m_initialized)
    {
        shutdown();
    }
}

bool
converter_context::init(const std::filesystem::path& data_root,
                        const std::filesystem::path& output_root)
{
    if (m_initialized)
    {
        ALOG_WARN("converter_context already initialized");
        return true;
    }

    glob::glob_state_reset();
    auto& gs = glob::glob_state();

    state_mutator__vfs::set(gs);

    auto& vfs = gs.getr_vfs();

    std::filesystem::path abs_data = std::filesystem::absolute(data_root);
    if (!std::filesystem::exists(abs_data))
    {
        ALOG_ERROR("Data root does not exist: {}", abs_data.string());
        return false;
    }
    vfs.mount("data", std::make_unique<vfs::physical_backend>(abs_data), 0);

    std::filesystem::path abs_output = std::filesystem::absolute(output_root);
    std::error_code ec;
    std::filesystem::create_directories(abs_output, ec);
    if (ec)
    {
        ALOG_ERROR("Failed to create output root: {}", abs_output.string());
        return false;
    }
    vfs.mount("output", std::make_unique<vfs::physical_backend>(abs_output), 0);

    ALOG_INFO("Setting up lua_api...");
    core::state_mutator__lua_api::set(gs);
    ALOG_INFO("Setting up model system...");
    core::state_mutator__model::set(gs);

    ALOG_INFO("Running create stage...");
    gs.run_create();

    ALOG_INFO("Initializing packages...");
    if (!init_packages())
    {
        ALOG_ERROR("Failed to initialize base packages");
        return false;
    }

    m_initialized = true;
    ALOG_INFO("converter_context initialized");
    return true;
}

bool
converter_context::init_packages()
{
    auto& gs = glob::glob_state();
    auto& pm = gs.getr_model().packages;

    ALOG_INFO("Loading root package...");
    {
        pm.register_static_package_loader<root::package>();
        auto& pkg = pm.load_static_package<root::package>();
        pkg.init();
        pkg.register_package_extension<root::package::package_types_builder>();
        pkg.complete_load();
    }
    ALOG_INFO("Root package loaded");

    ALOG_INFO("Loading base package...");
    {
        pm.register_static_package_loader<base::package>();
        auto& pkg = pm.load_static_package<base::package>();
        pkg.init();
        pkg.register_package_extension<base::package::package_types_builder>();
        pkg.complete_load();
    }
    ALOG_INFO("Base package loaded");

    return true;
}

void
converter_context::shutdown()
{
    if (!m_initialized)
    {
        return;
    }

    for (auto& lvl : m_created_levels)
    {
        lvl->unload();
    }
    m_created_levels.clear();

    for (auto& pkg : m_created_packages)
    {
        pkg->unload();
    }
    m_created_packages.clear();

    shutdown_packages();

    glob::glob_state_reset();
    m_initialized = false;

    ALOG_INFO("converter_context shutdown");
}

void
converter_context::shutdown_packages()
{
    base::package::instance().unload();
    root::package::instance().unload();
}

core::package*
converter_context::create_package(const utils::id& id)
{
    auto pkg = std::make_unique<core::package>(id);

    // Build OLC without VFS mount — converter writes .aobj files directly via
    // object_constructor::object_save in save_package().
    pkg->init_for_conversion();
    pkg->set_state(core::package_state::loaded);

    auto* ptr = pkg.get();
    m_created_packages.push_back(std::move(pkg));

    glob::glob_state().getr_model().packages.register_package(*ptr);

    return ptr;
}

core::package*
converter_context::load_package(const utils::id& id)
{
    auto& pm = glob::glob_state().getr_model().packages;

    ALOG_INFO("[converter] load_package: id=[{}]", id.str());

    if (!pm.load_package(id))
    {
        ALOG_ERROR("[converter] load_package FAILED: id=[{}]", id.str());
        return nullptr;
    }

    auto* pkg = pm.get_package(id);
    if (pkg)
    {
        log_package_contents(*pkg, "loaded from disk");
    }
    return pkg;
}

bool
converter_context::save_package(core::package& pkg, const vfs::rid& output_path)
{
    auto& vfs = glob::glob_state().getr_vfs();

    // output_path points to the package root (e.g. output://fox.apkg); parent is
    // the directory we write into. All directory creation below routes through
    // VFS — only resolve to fs paths where the downstream API (object_save,
    // buffer::set_file, pkg.set_save_root_path) requires one.
    auto parent_path = parent_of(output_path);
    std::string pkg_name = pkg.get_id().str() + ".apkg";
    auto pkg_rid = parent_path / pkg_name;

    if (!vfs.create_directories(pkg_rid))
    {
        ALOG_ERROR("Failed to create package dir: {}", pkg_rid.str());
        return false;
    }

    auto pkg_real = vfs.real_path(pkg_rid);
    if (!pkg_real.has_value())
    {
        ALOG_ERROR("Cannot resolve package path: {}", pkg_rid.str());
        return false;
    }
    auto pkg_dir = utils::path(pkg_real.value());

    pkg.set_vfs_root(pkg_rid);
    vfs.mount(pkg_rid, pkg_real.value(), {});

    // Assets with buffer properties (mesh vertices/indices, texture pixel data)
    // serialize the buffer's file path as the property value. For objects built
    // in memory, that path is empty — buffer__save() in types_handlers.cpp then
    // null-derefs via get_package()->get_relative_path(empty). Assign convention
    // file paths and the package's save_root_path before iterating.
    pkg.set_save_root_path(pkg_dir);

    auto bin_rid = pkg_rid / "binaries";
    if (!vfs.create_directories(bin_rid))
    {
        ALOG_ERROR("Failed to create binaries dir: {}", bin_rid.str());
        return false;
    }
    auto bin_dir = pkg_dir / "binaries";

    std::map<std::string, std::string> class_paths;

    auto objs = pkg.get_objects();
    for (auto& obj_sp : objs)
    {
        if (!obj_sp)
        {
            continue;
        }
        auto& obj = *obj_sp;

        if (auto* m = obj.as<root::mesh>())
        {
            m->get_vertices_buffer().set_file(bin_dir / (obj.get_id().str() + "_vertices.abin"));
            m->get_indices_buffer().set_file(bin_dir / (obj.get_id().str() + "_indices.abin"));
            m->get_external_buffer().set_file(bin_dir / (obj.get_id().str() + "_external.abin"));
        }
        else if (auto* t = obj.as<root::texture>())
        {
            t->get_mutable_base_color().set_file(bin_dir /
                                                 (obj.get_id().str() + "_base_color.abin"));
        }

        auto subdir = architype_subdir(obj.get_architype_id());
        if (subdir.empty())
        {
            ALOG_WARN("Skipping object with unmapped architype: {}", obj.get_id().str());
            continue;
        }

        auto subdir_rid = pkg_rid / subdir;
        if (!vfs.create_directories(subdir_rid))
        {
            ALOG_ERROR("Failed to create subdir: {}", subdir_rid.str());
            return false;
        }

        std::string rel_path = subdir + "/" + obj.get_id().str() + ".aobj";

        vfs.register_object(pkg_rid, obj.get_id().str(), rel_path);

        core::object_constructor ctor(&pkg.get_load_context());
        auto result = ctor.save_obj(obj);
        if (result != result_code::ok)
        {
            ALOG_ERROR("Failed to save object: {}", obj.get_id().str());
            return false;
        }

        class_paths[obj.get_id().str()] = rel_path;
    }

    auto meta_file = pkg_dir / "package.acfg";
    serialization::container meta;
    int i = 0;
    for (auto& [id, path] : class_paths)
    {
        meta["class_obj_mapping"][i++][id] = path;
    }

    for (const auto& dep : pkg.get_runtime_dependencies())
    {
        meta["dependencies"].push_back(dep.str());
    }

    if (!serialization::write_container(meta_file, meta))
    {
        ALOG_ERROR("Failed to write package.acfg");
        return false;
    }

    return true;
}

core::level*
converter_context::create_level(const utils::id& id, std::span<const utils::id> package_deps)
{
    auto lvl = std::make_unique<core::level>(id);

    for (const auto& dep : package_deps)
    {
        lvl->add_package_id(dep);
    }

    auto* ptr = lvl.get();
    m_created_levels.push_back(std::move(lvl));

    return ptr;
}

bool
converter_context::save_level(core::level& lvl, const vfs::rid& output_path)
{
    auto& vfs = glob::glob_state().getr_vfs();

    auto parent_path = parent_of(output_path);
    std::string lvl_name = lvl.get_id().str() + ".alvl";
    auto lvl_rid = parent_path / lvl_name;
    auto go_rid = lvl_rid / "game_objects";

    if (!vfs.create_directories(go_rid))
    {
        ALOG_ERROR("Failed to create level game_objects dir: {}", go_rid.str());
        return false;
    }

    auto lvl_real = vfs.real_path(lvl_rid);
    if (!lvl_real.has_value())
    {
        ALOG_ERROR("Cannot resolve level path: {}", lvl_rid.str());
        return false;
    }
    auto lvl_dir = utils::path(lvl_real.value());

    lvl.set_vfs_root(lvl_rid);
    vfs.mount(lvl_rid, lvl_real.value(), {});

    // Save game objects first so we can record their paths in root.cfg.
    serialization::container cfg;
    for (const auto& pkg_id : lvl.get_package_ids())
    {
        cfg["packages"].push_back(pkg_id.str() + ".apkg");
    }

    auto& local_cache = lvl.get_local_cache();
    for (auto& [id, obj] : local_cache.game_objects.get_items())
    {
        if (!obj)
        {
            continue;
        }
        std::string rel_path = "game_objects/" + id.str() + ".aobj";

        vfs.register_object(lvl_rid, id.str(), rel_path);

        core::object_constructor ctor(&lvl.get_load_context());
        auto result = ctor.save_obj(*obj);
        if (result != result_code::ok)
        {
            ALOG_ERROR("Failed to save game object: {}", id.str());
            return false;
        }

        cfg["instance_obj_mapping"][id.str()] = rel_path;
    }

    auto root_path = lvl_dir / "root.cfg";
    if (!serialization::write_container(root_path, cfg))
    {
        ALOG_ERROR("Failed to write root.cfg");
        return false;
    }

    return true;
}

root::mesh*
converter_context::create_mesh(core::package& pkg, const utils::id& id, const parsed_mesh& data)
{
    auto& olc = pkg.get_load_context();

    root::mesh::construct_params params;
    params.vertices.write(data.vertices.data(), data.vertices.size());
    params.indices.write(reinterpret_cast<const uint8_t*>(data.indices.data()),
                         data.indices.size() * sizeof(uint32_t));

    core::object_constructor ctor(&olc, core::object_load_type::class_obj);
    auto result = ctor.construct_obj(root::mesh::AR_TYPE_id(), id, params, false);

    if (!result)
    {
        ALOG_ERROR("Failed to create mesh: {}", id.str());
        return nullptr;
    }

    return result.value()->as<root::mesh>();
}

root::texture*
converter_context::create_texture(core::package& pkg,
                                  const utils::id& id,
                                  const parsed_texture& data)
{
    auto& olc = pkg.get_load_context();

    root::texture::construct_params params;

    core::object_constructor ctor(&olc, core::object_load_type::class_obj);
    auto result = ctor.construct_obj(root::texture::AR_TYPE_id(), id, params, false);

    if (!result)
    {
        ALOG_ERROR("Failed to create texture: {}", id.str());
        return nullptr;
    }

    auto* tex = result.value()->as<root::texture>();

    tex->set_width(data.width);
    tex->set_height(data.height);

    if (data.embedded && !data.pixels.empty())
    {
        tex->get_mutable_base_color().write(data.pixels.data(), data.pixels.size());
    }
    else if (!data.source_path.empty())
    {
        std::ifstream file(data.source_path, std::ios::binary | std::ios::ate);
        if (file)
        {
            auto size = file.tellg();
            file.seekg(0, std::ios::beg);
            std::vector<uint8_t> file_data(static_cast<size_t>(size));
            if (file.read(reinterpret_cast<char*>(file_data.data()), size))
            {
                tex->get_mutable_base_color().write(file_data.data(), file_data.size());
            }
        }
        else
        {
            ALOG_WARN("Failed to read texture file: {}", data.source_path);
        }
    }

    return tex;
}

base::pbr_material*
converter_context::create_material(core::package& pkg,
                                   const utils::id& id,
                                   const parsed_material& data,
                                   root::texture* diffuse_tex,
                                   root::texture* specular_tex)
{
    auto& olc = pkg.get_load_context();

    base::pbr_material::construct_params params;

    core::object_constructor ctor(&olc, core::object_load_type::class_obj);
    auto result = ctor.construct_obj(base::pbr_material::AR_TYPE_id(), id, params, false);

    if (!result)
    {
        ALOG_ERROR("Failed to create material: {}", id.str());
        return nullptr;
    }

    auto* mat = result.value()->as<base::pbr_material>();

    mat->set_ambient(root::vec3(data.ambient));
    mat->set_diffuse(root::vec3(data.diffuse));
    mat->set_specular(root::vec3(data.specular));
    mat->m_shininess = data.shininess;

    if (diffuse_tex)
    {
        root::texture_slot slot;
        slot.txt = diffuse_tex;
        slot.slot = 0;
        mat->m_diffuse_txt = slot;
    }

    if (specular_tex)
    {
        root::texture_slot slot;
        slot.txt = specular_tex;
        slot.slot = 1;
        mat->m_specular_txt = slot;
    }

    auto* shader_effect = olc.find_obj(AID(data.shader_effect));
    if (shader_effect)
    {
        mat->set_shader_effect(shader_effect->as<root::shader_effect>());
    }
    else
    {
        ALOG_WARN("Shader effect not found: {}", data.shader_effect);
    }

    return mat;
}

// (Old per-class place_* helpers removed — see place_scene_root +
//  spawn_node_component below.)

std::string
converter_context::make_id(const converter_options& opts, const std::string& name) const
{
    if (opts.prefix.empty())
    {
        return name;
    }
    return opts.prefix + name;
}

std::string
converter_context::make_texture_id(const converter_options& opts, const std::string& name) const
{
    std::string id = make_id(opts, name);
    if (id.substr(0, 4) != "txt_")
    {
        id = "txt_" + id;
    }
    return id;
}

std::string
converter_context::make_material_id(const converter_options& opts, const std::string& name) const
{
    std::string id = make_id(opts, name);
    if (id.substr(0, 3) != "mt_")
    {
        id = "mt_" + id;
    }
    return id;
}

bool
converter_context::create_package_assets(core::package& pkg,
                                         const parsed_scene& scene,
                                         const converter_options& opts)
{
    auto& olc = pkg.get_load_context();
    int tex_created = 0, tex_skipped = 0;
    int mat_created = 0, mat_skipped = 0;
    int mesh_created = 0;

    ALOG_INFO(
        "[converter] create_package_assets: pkg=[{}] prefix=[{}] scene: {} tex, {} mat, {} mesh",
        pkg.get_id().str(),
        opts.prefix,
        scene.textures.size(),
        scene.materials.size(),
        scene.meshes.size());

    for (const auto& tex_data : scene.textures)
    {
        std::string tex_id = make_texture_id(opts, tex_data.name);

        if (opts.deduplicate_textures && olc.find_obj(AID(tex_id)))
        {
            ALOG_INFO("[converter]   tex skip (dedup): name=[{}] id=[{}]", tex_data.name, tex_id);
            ++tex_skipped;
            continue;
        }

        auto* tex = create_texture(pkg, AID(tex_id), tex_data);
        if (tex)
        {
            ALOG_INFO("[converter]   tex ok: name=[{}] id=[{}] {}x{} embedded={}",
                      tex_data.name,
                      tex_id,
                      tex_data.width,
                      tex_data.height,
                      tex_data.embedded);
            ++tex_created;
        }
        else
        {
            ALOG_WARN("[converter]   tex FAIL: name=[{}] id=[{}]", tex_data.name, tex_id);
        }
    }

    for (const auto& mat_data : scene.materials)
    {
        std::string mat_id = make_material_id(opts, mat_data.name);

        if (opts.deduplicate_materials && olc.find_obj(AID(mat_id)))
        {
            ALOG_INFO("[converter]   mat skip (dedup): name=[{}] id=[{}]", mat_data.name, mat_id);
            ++mat_skipped;
            continue;
        }

        root::texture* diffuse_tex = nullptr;
        root::texture* specular_tex = nullptr;

        if (!mat_data.diffuse_texture.empty())
        {
            std::string tex_id = make_texture_id(opts, mat_data.diffuse_texture);
            auto* proto = olc.find_obj(AID(tex_id));
            if (proto)
            {
                diffuse_tex = proto->as<root::texture>();
            }
            else
            {
                ALOG_WARN(
                    "[converter]     mat [{}] diffuse tex [{}] not found in pkg", mat_id, tex_id);
            }
        }

        if (!mat_data.specular_texture.empty())
        {
            std::string tex_id = make_texture_id(opts, mat_data.specular_texture);
            auto* proto = olc.find_obj(AID(tex_id));
            if (proto)
            {
                specular_tex = proto->as<root::texture>();
            }
            else
            {
                ALOG_WARN(
                    "[converter]     mat [{}] specular tex [{}] not found in pkg", mat_id, tex_id);
            }
        }

        auto* mat = create_material(pkg, AID(mat_id), mat_data, diffuse_tex, specular_tex);
        if (mat)
        {
            ALOG_INFO("[converter]   mat ok: name=[{}] id=[{}] shader=[{}]",
                      mat_data.name,
                      mat_id,
                      mat_data.shader_effect);
            ++mat_created;
        }
        else
        {
            ALOG_WARN("[converter]   mat FAIL: name=[{}] id=[{}]", mat_data.name, mat_id);
        }
    }

    for (const auto& mesh_data : scene.meshes)
    {
        std::string mesh_id = make_id(opts, mesh_data.name);
        auto* mesh = create_mesh(pkg, AID(mesh_id), mesh_data);
        if (mesh)
        {
            ALOG_INFO("[converter]   mesh ok: name=[{}] id=[{}] vbytes={} indices={}",
                      mesh_data.name,
                      mesh_id,
                      mesh_data.vertices.size(),
                      mesh_data.indices.size());
            ++mesh_created;
        }
        else
        {
            ALOG_WARN("[converter]   mesh FAIL: name=[{}] id=[{}]", mesh_data.name, mesh_id);
        }
    }

    ALOG_INFO(
        "[converter] create_package_assets done: pkg=[{}] {} tex ({} skipped), {} mat ({} "
        "skipped), {} mesh",
        pkg.get_id().str(),
        tex_created,
        tex_skipped,
        mat_created,
        mat_skipped,
        mesh_created);

    return true;
}

namespace
{

// Lookup helpers shared between place_scene_root and spawn_node_component.
root::mesh*
lookup_node_mesh(core::package& pkg,
                 const parsed_scene& scene,
                 const converter_options& opts,
                 const parsed_node& node,
                 const std::string& (*log_id)(const std::string&) = nullptr)
{
    (void)log_id;
    if (node.mesh_index < 0 || size_t(node.mesh_index) >= scene.meshes.size())
    {
        return nullptr;
    }
    const auto& md = scene.meshes[node.mesh_index];
    std::string mesh_id = opts.prefix.empty() ? md.name : opts.prefix + md.name;
    auto* proto = pkg.get_load_context().find_obj(utils::id::make_id(mesh_id));
    return proto ? proto->as<root::mesh>() : nullptr;
}

root::material*
lookup_node_material(core::package& pkg,
                     const parsed_scene& scene,
                     const converter_options& opts,
                     const parsed_node& node)
{
    if (node.material_index < 0 || size_t(node.material_index) >= scene.materials.size())
    {
        return nullptr;
    }
    const auto& mt = scene.materials[node.material_index];
    std::string mat_id = opts.prefix.empty() ? mt.name : opts.prefix + mt.name;
    if (mat_id.substr(0, 3) != "mt_")
    {
        mat_id = "mt_" + mat_id;
    }
    auto* proto = pkg.get_load_context().find_obj(utils::id::make_id(mat_id));
    return proto ? proto->as<root::material>() : nullptr;
}

void
configure_point_light(base::point_light_component* lc, const parsed_light& data)
{
    glm::vec3 diffuse = data.color * std::min(data.intensity, 1.0f);
    glm::vec3 ambient = diffuse * 0.1f;
    lc->set_ambient(root::vec3(ambient));
    lc->set_diffuse(root::vec3(diffuse));
    lc->set_specular(root::vec3(diffuse));
    lc->set_radius(data.range);
}

void
configure_directional_light(base::directional_light_component* lc, const parsed_light& data)
{
    glm::vec3 diffuse = data.color * std::min(data.intensity, 1.0f);
    glm::vec3 ambient = diffuse * 0.1f;
    lc->set_ambient(root::vec3(ambient));
    lc->set_diffuse(root::vec3(diffuse));
    lc->set_specular(root::vec3(diffuse));
    lc->set_direction(root::vec3(glm::vec3(0.f, -1.f, 0.f)));
}

void
configure_spot_light(base::spot_light_component* lc, const parsed_light& data)
{
    glm::vec3 diffuse = data.color * std::min(data.intensity, 1.0f);
    glm::vec3 ambient = diffuse * 0.1f;
    lc->set_ambient(root::vec3(ambient));
    lc->set_diffuse(root::vec3(diffuse));
    lc->set_specular(root::vec3(diffuse));
    lc->set_direction(root::vec3(glm::vec3(0.f, -1.f, 0.f)));
    lc->set_radius(data.range);
    lc->set_cut_off(glm::degrees(data.inner_cone));
    lc->set_outer_cut_off(glm::degrees(data.outer_cone));
}

void
configure_camera(base::camera_component* cc, const parsed_camera& data)
{
    cc->set_perspective(data.fov, data.aspect_ratio, data.znear, data.zfar);
}

}  // namespace

root::component*
converter_context::spawn_node_component(root::game_object* obj,
                                        root::component* parent,
                                        core::package& pkg,
                                        const parsed_scene& scene,
                                        const converter_options& opts,
                                        int node_idx)
{
    if (node_idx < 0 || static_cast<size_t>(node_idx) >= scene.nodes.size())
    {
        return nullptr;
    }

    const auto& node = scene.nodes[node_idx];
    auto cid = utils::id::make_id(make_id(opts, node.name));

    root::component* this_comp = nullptr;

    if (node.mesh_index >= 0 && static_cast<size_t>(node.mesh_index) < scene.meshes.size())
    {
        auto* mesh = lookup_node_mesh(pkg, scene, opts, node);
        if (!mesh)
        {
            ALOG_WARN("[converter]   MESH NOT FOUND for node [{}]", node.name);
            return nullptr;
        }
        auto* material = lookup_node_material(pkg, scene, opts, node);

        base::mesh_component::construct_params p;
        p.mesh_handle = mesh;
        p.material_handle = material;
        this_comp = obj->spawn_component<base::mesh_component>(parent, cid, p);
    }
    else if (node.light_index >= 0 && size_t(node.light_index) < scene.lights.size())
    {
        const auto& light = scene.lights[node.light_index];
        switch (light.type)
        {
        case parsed_light_type::point:
        {
            auto* lc = obj->spawn_component<base::point_light_component>(parent, cid, {});
            if (lc)
            {
                configure_point_light(lc, light);
            }
            this_comp = lc;
            break;
        }
        case parsed_light_type::directional:
        {
            auto* lc = obj->spawn_component<base::directional_light_component>(parent, cid, {});
            if (lc)
            {
                configure_directional_light(lc, light);
            }
            this_comp = lc;
            break;
        }
        case parsed_light_type::spot:
        {
            auto* lc = obj->spawn_component<base::spot_light_component>(parent, cid, {});
            if (lc)
            {
                configure_spot_light(lc, light);
            }
            this_comp = lc;
            break;
        }
        }
    }
    else if (node.camera_index >= 0 && size_t(node.camera_index) < scene.cameras.size())
    {
        const auto& camera = scene.cameras[node.camera_index];
        auto* cc = obj->spawn_component<base::camera_component>(parent, cid, {});
        if (cc)
        {
            configure_camera(cc, camera);
        }
        this_comp = cc;
    }
    else
    {
        // Pure transform node — game_object_component preserves the node's
        // transform so descendants compose correctly.
        this_comp = obj->spawn_component<root::game_object_component>(parent, cid, {});
    }

    if (!this_comp)
    {
        return nullptr;
    }

    // Apply node's local transform via the inherited game_object_component
    // setters — avoids the camera_component::construct_params::scale (mat4)
    // that hides the vec3 from the base.
    if (auto* goc = this_comp->as<root::game_object_component>())
    {
        goc->set_position(root::vec3(node.transform.position));
        goc->set_rotation(root::vec3(node.transform.rotation));
        goc->set_scale(root::vec3(node.transform.scale));
    }

    for (int child_idx : node.children)
    {
        spawn_node_component(obj, this_comp, pkg, scene, opts, child_idx);
    }

    return this_comp;
}

void
converter_context::place_scene_root(core::level& lvl,
                                    core::package& pkg,
                                    const parsed_scene& scene,
                                    const converter_options& opts,
                                    int root_idx)
{
    if (root_idx < 0 || static_cast<size_t>(root_idx) >= scene.nodes.size())
    {
        return;
    }

    const auto& root_node = scene.nodes[root_idx];
    auto obj_id = utils::id::make_id(make_id(opts, root_node.name));

    ALOG_INFO("[converter] place scene root[{}] name=[{}] mesh={} light={} camera={} children={}",
              root_idx,
              root_node.name,
              root_node.mesh_index,
              root_node.light_index,
              root_node.camera_index,
              root_node.children.size());

    // Pick the umbrella class from the root node's payload. Single-payload
    // single-node scenes get the convenience subclass (mesh_object/point_light
    // /directional_light/spot_light/camera_object); transform-only roots get
    // plain game_object.
    utils::id cdo_id = AID("game_object");
    if (root_node.mesh_index >= 0 && size_t(root_node.mesh_index) < scene.meshes.size())
    {
        cdo_id = AID("mesh_object");
    }
    else if (root_node.light_index >= 0 && size_t(root_node.light_index) < scene.lights.size())
    {
        switch (scene.lights[root_node.light_index].type)
        {
        case parsed_light_type::point:
            cdo_id = AID("point_light");
            break;
        case parsed_light_type::directional:
            cdo_id = AID("directional_light");
            break;
        case parsed_light_type::spot:
            cdo_id = AID("spot_light");
            break;
        }
    }
    else if (root_node.camera_index >= 0 && size_t(root_node.camera_index) < scene.cameras.size())
    {
        cdo_id = AID("camera_object");
    }

    auto& olc = lvl.get_load_context();
    auto* cdo = olc.find_obj(cdo_id);
    if (!cdo)
    {
        ALOG_ERROR("CDO not found for scene root [{}]: {}", root_node.name, cdo_id.str());
        return;
    }

    core::object_constructor ctor(&olc, core::object_load_type::instance_obj);
    auto result = ctor.clone_obj(*cdo, obj_id);
    if (!result)
    {
        ALOG_ERROR("Failed to clone game_object for scene root [{}]", root_node.name);
        return;
    }

    auto* obj = result.value()->as<root::game_object>();
    if (!obj)
    {
        ALOG_ERROR("Cloned object is not a game_object: {}", obj_id.str());
        return;
    }

    auto* root_comp = obj->get_root_component();
    KRG_check(root_comp, "game_object must have a root component after clone");

    // Root component carries the root node's transform. Payload components
    // (mesh/light/camera) sit at identity local under it, so descendants and
    // payload share the same world transform via root_comp.
    root_comp->set_position(root::vec3(root_node.transform.position));
    root_comp->set_rotation(root::vec3(root_node.transform.rotation));
    root_comp->set_scale(root::vec3(root_node.transform.scale));

    // Attach payload to root_component:
    //   * mesh root → spawn mesh_component manually (mesh_object doesn't auto-spawn)
    //   * light/camera root → light/camera_object's construct() already auto-spawned
    //     the matching component during clone; just configure it.
    if (cdo_id == AID("mesh_object"))
    {
        if (auto* mesh = lookup_node_mesh(pkg, scene, opts, root_node))
        {
            auto* material = lookup_node_material(pkg, scene, opts, root_node);
            base::mesh_component::construct_params p;
            p.mesh_handle = mesh;
            p.material_handle = material;
            auto mc_id = utils::id::make_id(obj_id.str() + "_mesh");
            obj->spawn_component<base::mesh_component>(root_comp, mc_id, p);
        }
        else
        {
            ALOG_WARN("[converter]   MESH NOT FOUND for root node [{}]", root_node.name);
        }
    }
    else if (cdo_id == AID("point_light") || cdo_id == AID("directional_light") ||
             cdo_id == AID("spot_light"))
    {
        const auto& light = scene.lights[root_node.light_index];
        for (auto& comp : obj->get_components())
        {
            if (light.type == parsed_light_type::point)
            {
                if (auto* lc = comp.as<base::point_light_component>())
                {
                    configure_point_light(lc, light);
                    break;
                }
            }
            else if (light.type == parsed_light_type::directional)
            {
                if (auto* lc = comp.as<base::directional_light_component>())
                {
                    configure_directional_light(lc, light);
                    break;
                }
            }
            else if (light.type == parsed_light_type::spot)
            {
                if (auto* lc = comp.as<base::spot_light_component>())
                {
                    configure_spot_light(lc, light);
                    break;
                }
            }
        }
    }
    else if (cdo_id == AID("camera_object"))
    {
        const auto& camera = scene.cameras[root_node.camera_index];
        for (auto& comp : obj->get_components())
        {
            if (auto* cc = comp.as<base::camera_component>())
            {
                configure_camera(cc, camera);
                break;
            }
        }
    }

    // Recurse children — their components live under root_comp's tree.
    for (int child_idx : root_node.children)
    {
        spawn_node_component(obj, root_comp, pkg, scene, opts, child_idx);
    }

    obj->recreate_structure_from_layout();
}

bool
converter_context::place_level_instances(core::level& lvl,
                                         core::package& pkg,
                                         const parsed_scene& scene,
                                         const converter_options& opts)
{
    int placed_count = 0;

    for (int root_idx : scene.root_nodes)
    {
        place_scene_root(lvl, pkg, scene, opts, root_idx);
        ++placed_count;
    }

    ALOG_INFO("Placed {} root nodes in level {}", placed_count, lvl.get_id().str());
    return true;
}

bool
converter_context::convert(const parsed_scene& scene, const converter_options& opts)
{
    if (!m_initialized)
    {
        ALOG_ERROR("converter_context not initialized");
        return false;
    }

    ALOG_INFO("[converter] convert scene=[{}] mode={} name=[{}] existing=[{}] prefix=[{}] deps={}",
              scene.name,
              static_cast<int>(opts.mode),
              opts.name.str(),
              opts.existing_package_id.str(),
              opts.prefix,
              opts.dependencies.size());
    for (const auto& d : opts.dependencies)
    {
        ALOG_INFO("[converter]   dep [{}]", d.str());
    }

    switch (opts.mode)
    {
    case converter_mode::package_new:
    {
        auto* pkg = create_package(opts.name);
        if (!pkg)
        {
            ALOG_ERROR("Failed to create package: {}", opts.name.str());
            return false;
        }

        pkg->set_runtime_dependencies(opts.dependencies);

        if (!create_package_assets(*pkg, scene, opts))
        {
            return false;
        }

        auto pkg_path = opts.output_root / (opts.name.str() + ".apkg");
        if (!save_package(*pkg, pkg_path))
        {
            ALOG_ERROR("Failed to save package to: {}", pkg_path.str());
            return false;
        }

        ALOG_INFO("Package saved: {}", pkg_path.str());
        return true;
    }

    case converter_mode::package_extend:
    {
        auto* pkg = load_package(opts.existing_package_id);
        if (!pkg)
        {
            ALOG_ERROR("Failed to load existing package: {}", opts.existing_package_id.str());
            return false;
        }

        // Merge existing deps with opts.dependencies (dedup, preserve order).
        std::vector<utils::id> merged = pkg->get_runtime_dependencies();
        for (const auto& d : opts.dependencies)
        {
            if (std::find(merged.begin(), merged.end(), d) == merged.end())
            {
                merged.push_back(d);
            }
        }
        pkg->set_runtime_dependencies(std::move(merged));

        if (!create_package_assets(*pkg, scene, opts))
        {
            return false;
        }

        auto pkg_path = opts.output_root / (opts.existing_package_id.str() + ".apkg");
        if (!save_package(*pkg, pkg_path))
        {
            ALOG_ERROR("Failed to save extended package to: {}", pkg_path.str());
            return false;
        }

        ALOG_INFO("Extended package saved: {}", pkg_path.str());
        return true;
    }

    case converter_mode::level_standalone:
    {
        auto* pkg = load_package(opts.existing_package_id);
        if (!pkg)
        {
            ALOG_ERROR("Failed to load package: {}", opts.existing_package_id.str());
            return false;
        }

        // Level depends on opts.dependencies plus the asset-source package.
        std::vector<utils::id> deps = opts.dependencies;
        if (std::find(deps.begin(), deps.end(), opts.existing_package_id) == deps.end())
        {
            deps.push_back(opts.existing_package_id);
        }

        auto* lvl = create_level(opts.name, deps);
        if (!lvl)
        {
            ALOG_ERROR("Failed to create level: {}", opts.name.str());
            return false;
        }

        if (!place_level_instances(*lvl, *pkg, scene, opts))
        {
            return false;
        }

        auto lvl_path = opts.output_root / (opts.name.str() + ".alvl");
        if (!save_level(*lvl, lvl_path))
        {
            ALOG_ERROR("Failed to save level to: {}", lvl_path.str());
            return false;
        }

        ALOG_INFO("Level saved: {}", lvl_path.str());
        return true;
    }

    case converter_mode::level_with_package:
    {
        auto* pkg = create_package(opts.name);
        if (!pkg)
        {
            ALOG_ERROR("Failed to create package: {}", opts.name.str());
            return false;
        }

        pkg->set_runtime_dependencies(opts.dependencies);

        if (!create_package_assets(*pkg, scene, opts))
        {
            return false;
        }

        // Level depends on opts.dependencies plus the newly-created package.
        std::vector<utils::id> deps = opts.dependencies;
        if (std::find(deps.begin(), deps.end(), opts.name) == deps.end())
        {
            deps.push_back(opts.name);
        }

        auto* lvl = create_level(opts.name, deps);
        if (!lvl)
        {
            ALOG_ERROR("Failed to create level: {}", opts.name.str());
            return false;
        }

        if (!place_level_instances(*lvl, *pkg, scene, opts))
        {
            return false;
        }

        auto pkg_path = opts.output_root / (opts.name.str() + ".apkg");
        auto lvl_path = opts.output_root / (opts.name.str() + ".alvl");

        if (!save_package(*pkg, pkg_path))
        {
            ALOG_ERROR("Failed to save package to: {}", pkg_path.str());
            return false;
        }

        if (!save_level(*lvl, lvl_path))
        {
            ALOG_ERROR("Failed to save level to: {}", lvl_path.str());
            return false;
        }

        ALOG_INFO("Package and level saved: {}, {}", pkg_path.str(), lvl_path.str());
        return true;
    }
    }

    ALOG_ERROR("Unknown converter mode: {}", static_cast<int>(opts.mode));
    return false;
}

}  // namespace kryga::converter
