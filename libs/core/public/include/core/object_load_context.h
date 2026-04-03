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
    friend class object_load_context_builder;

public:
    object_load_context();
    ~object_load_context();

    bool
    resolve(const utils::id& id, vfs::rid& out) const;

    bool
    resolve(const utils::path& relative, vfs::rid& out) const;

    bool
    add_obj(std::shared_ptr<root::smart_object> obj);

    bool
    remove_obj(const root::smart_object& obj);

    root::smart_object*
    find_proto_obj(const utils::id& id);

    root::smart_object*
    find_obj(const utils::id& id);

    root::smart_object*
    find_proto_obj(const utils::id& id, architype a_type);

    root::smart_object*
    find_obj(const utils::id& id, architype a_type);

    // clang-format off
    object_load_context& set_vfs_mount (const vfs::rid& v)       { m_vfs_root = v; return *this; }

    cache_set*              get_instance_local_set() const  { return m_instance_local_set; }
    package*                get_package() const             { return m_package; }
    const vfs::rid&         get_vfs_root() const            { return m_vfs_root; }
    level*                  get_level() const               { return m_level; }

    // clang-format on
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
    vfs::rid m_vfs_root;

    cache_set* m_proto_local_set = nullptr;
    cache_set* m_instance_local_set = nullptr;

    package* m_package = nullptr;
    level* m_level = nullptr;

    std::vector<root::smart_object*> m_loaded_objects;

    line_cache<root::smart_object_ptr>* m_ownable_cache_ptr;
};
}  // namespace core
}  // namespace kryga
