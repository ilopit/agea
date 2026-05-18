#pragma once

#include "core/model_minimal.h"
#include "core/model_fwds.h"
#include "core/caches/cache_set.h"
#include "core/caches/line_cache.h"

#include <vfs/rid.h>

namespace kryga
{
namespace core
{

class object_load_context
{
public:
    object_load_context();
    ~object_load_context();

    // --- VFS resolution ---

    bool
    resolve(const utils::id& id, vfs::rid& out) const;

    bool
    resolve(const utils::path& relative, vfs::rid& out) const;

    // --- Object storage ---
    // Registers an object for lifetime management and caching.
    // Class objects (readonly) enter the global + local cache.
    // Instance objects are stored for lifetime only — they're accessed through their parent.
    bool
    add_obj(std::shared_ptr<root::smart_object> obj);

    bool
    remove_obj(const root::smart_object& obj);

    root::smart_object*
    find_obj(const utils::id& id);

    root::smart_object*
    find_obj(const utils::id& id, architype a_type);

    // --- Accessors ---

    object_load_context&
    set_vfs_mount(const vfs::rid& v)
    {
        m_vfs_root = v;
        return *this;
    }

    object_load_context&
    set_local_set(cache_set* v)
    {
        m_local_set = v;
        return *this;
    }

    object_load_context&
    set_ownable_cache(line_cache<root::smart_object_ptr>* v)
    {
        m_ownable_cache_ptr = v;
        return *this;
    }

    object_load_context&
    set_package(package* v)
    {
        m_package = v;
        return *this;
    }

    object_load_context&
    set_level(level* v)
    {
        m_level = v;
        return *this;
    }

    cache_set*
    get_local_set() const
    {
        return m_local_set;
    }

    package*
    get_package() const
    {
        return m_package;
    }

    const vfs::rid&
    get_vfs_root() const
    {
        return m_vfs_root;
    }

    level*
    get_level() const
    {
        return m_level;
    }

    // --- Loaded objects tracking ---

    void
    reset_loaded_objects(std::vector<root::smart_object*>& old_object,
                         std::vector<root::smart_object*>& result)
    {
        result = std::move(m_loaded_objects);
        m_loaded_objects = std::move(old_object);
    }

    void
    reset_loaded_objects(std::vector<root::smart_object*>& old_object)
    {
        m_loaded_objects = std::move(old_object);
    }

    std::vector<root::smart_object*>
    reset_loaded_objects()
    {
        return std::move(m_loaded_objects);
    }

    void
    push_object_loaded(root::smart_object* o)
    {
        m_loaded_objects.push_back(o);
    }

private:
    friend class object_load_context_builder;

    vfs::rid m_vfs_root;
    cache_set* m_local_set = nullptr;
    package* m_package = nullptr;
    level* m_level = nullptr;
    std::vector<root::smart_object*> m_loaded_objects;
    line_cache<root::smart_object_ptr>* m_ownable_cache_ptr = nullptr;
};

}  // namespace core
}  // namespace kryga
