#pragma once

#include <utils/id.h>
#include <vfs/rid.h>

#include <glm/glm.hpp>

#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace kryga
{

namespace core
{
class package;
class level;
}  // namespace core

namespace root
{
class mesh;
class texture;
class material;
class shader_effect;
class game_object;
class component;
}  // namespace root

namespace base
{
class pbr_material;
}  // namespace base

namespace base
{
class mesh_object;
class point_light;
class directional_light;
class spot_light;
class camera_object;
}  // namespace base

namespace converter
{

// ============================================================================
// Parsed data types (raw data from parsers, not engine objects)
// ============================================================================

struct parsed_mesh
{
    std::string name;
    std::vector<uint8_t> vertices;  // raw vertex_data bytes
    std::vector<uint32_t> indices;
};

struct parsed_texture
{
    std::string name;
    std::string source_path;      // external file path
    std::vector<uint8_t> pixels;  // RGBA8 when embedded
    uint32_t width = 0;
    uint32_t height = 0;
    bool embedded = false;
};

struct parsed_material
{
    std::string name;
    std::string shader_effect = "se_simple_texture_lit";
    std::string diffuse_texture;  // texture name reference
    std::string specular_texture;
    glm::vec3 ambient = {0.2f, 0.2f, 0.2f};
    glm::vec3 diffuse = {0.8f, 0.8f, 0.8f};
    glm::vec3 specular = {0.5f, 0.5f, 0.5f};
    float shininess = 64.0f;
};

enum class parsed_light_type
{
    point,
    directional,
    spot
};

struct parsed_light
{
    std::string name;
    parsed_light_type type = parsed_light_type::point;
    glm::vec3 color = {1.f, 1.f, 1.f};
    float intensity = 1.f;
    float range = 50.f;
    float inner_cone = 0.f;
    float outer_cone = 0.7854f;
};

struct parsed_camera
{
    std::string name;
    float fov = 60.f;
    float znear = 0.1f;
    float zfar = 256.f;
    float aspect_ratio = 16.f / 9.f;
};

struct parsed_transform
{
    glm::vec3 position = {0.f, 0.f, 0.f};
    glm::vec3 rotation = {0.f, 0.f, 0.f};  // euler degrees
    glm::vec3 scale = {1.f, 1.f, 1.f};
};

struct parsed_node
{
    std::string name;
    parsed_transform transform;
    int mesh_index = -1;
    int material_index = -1;
    int light_index = -1;
    int camera_index = -1;
    std::vector<int> children;
};

struct parsed_scene
{
    std::string name;
    std::string source_path;
    std::vector<parsed_mesh> meshes;
    std::vector<parsed_texture> textures;
    std::vector<parsed_material> materials;
    std::vector<parsed_light> lights;
    std::vector<parsed_camera> cameras;
    std::vector<parsed_node> nodes;
    std::vector<int> root_nodes;
};

// ============================================================================
// Converter options
// ============================================================================

enum class converter_mode
{
    package_new,        // Create new package with assets
    package_extend,     // Add assets to existing package
    level_standalone,   // Create level using existing package
    level_with_package  // Create both package and level
};

struct converter_options
{
    converter_mode mode = converter_mode::package_new;
    utils::id name;        // output name (scene.apkg / scene.alvl)
    vfs::rid output_root;  // e.g., output://packages/

    // For package_extend / level_standalone
    utils::id existing_package_id;

    // Asset handling
    bool deduplicate_textures = true;
    bool deduplicate_materials = true;

    // Naming
    std::string prefix;  // prefix for generated IDs

    // Dependencies written into the output package/level. Applies to package
    // modes (package_new / level_with_package) and level modes (level_standalone
    // / level_with_package). 'root' and 'base' should typically be included.
    std::vector<utils::id> dependencies;
};

// ============================================================================
// Converter context - headless engine environment
// ============================================================================

class converter_context
{
public:
    converter_context();
    ~converter_context();

    converter_context(const converter_context&) = delete;
    converter_context&
    operator=(const converter_context&) = delete;

    // Lifecycle
    bool
    init(const std::filesystem::path& data_root, const std::filesystem::path& output_root);
    void
    shutdown();

    // Package operations
    core::package*
    create_package(const utils::id& id);
    core::package*
    load_package(const utils::id& id);
    bool
    save_package(core::package& pkg, const vfs::rid& output_path);

    // Level operations
    core::level*
    create_level(const utils::id& id, std::span<const utils::id> package_deps);
    bool
    save_level(core::level& lvl, const vfs::rid& output_path);

    // Asset construction (into specified package)
    root::mesh*
    create_mesh(core::package& pkg, const utils::id& id, const parsed_mesh& data);

    root::texture*
    create_texture(core::package& pkg, const utils::id& id, const parsed_texture& data);

    base::pbr_material*
    create_material(core::package& pkg,
                    const utils::id& id,
                    const parsed_material& data,
                    root::texture* diffuse_tex,
                    root::texture* specular_tex);

    // High-level conversion
    bool
    convert(const parsed_scene& scene, const converter_options& opts);

private:
    bool
    init_packages();
    void
    shutdown_packages();

    std::string
    make_id(const converter_options& opts, const std::string& name) const;
    std::string
    make_texture_id(const converter_options& opts, const std::string& name) const;
    std::string
    make_material_id(const converter_options& opts, const std::string& name) const;

    // Conversion stages
    bool
    create_package_assets(core::package& pkg,
                          const parsed_scene& scene,
                          const converter_options& opts);

    bool
    place_level_instances(core::level& lvl,
                          core::package& pkg,
                          const parsed_scene& scene,
                          const converter_options& opts);

    // Place one glTF scene root as a single game_object whose component
    // tree mirrors the node tree below. Umbrella class is picked from the
    // root node's payload (mesh_object / point_light / etc., or plain
    // game_object for transform-only roots).
    void
    place_scene_root(core::level& lvl,
                     core::package& pkg,
                     const parsed_scene& scene,
                     const converter_options& opts,
                     int root_idx);

    // Spawn one component for a non-root glTF node into `obj`'s tree as a
    // child of `parent`. Component subclass is determined by node payload.
    // Recurses into children of the node with the spawned component as the
    // new parent. Returns the spawned component, or nullptr on failure.
    root::component*
    spawn_node_component(root::game_object* obj,
                         root::component* parent,
                         core::package& pkg,
                         const parsed_scene& scene,
                         const converter_options& opts,
                         int node_idx);

    bool m_initialized = false;
    std::vector<std::unique_ptr<core::package>> m_created_packages;
    std::vector<std::unique_ptr<core::level>> m_created_levels;
};

}  // namespace converter
}  // namespace kryga
