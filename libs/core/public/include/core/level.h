#pragma once

#include "core/model_fwds.h"
#include "core/caches/cache_set.h"
#include "core/caches/line_cache.h"
#include "core/model_minimal.h"
#include "core/container.h"

namespace kryga::vfs { class backend; }

#include <packages/root/model/core_types/vec3.h>

#include <utils/singleton_instance.h>

#include <map>
#include <vector>
#include <string>
#include <optional>

namespace kryga
{
namespace core
{
struct spawn_parameters
{
    std::optional<root::vec3> position;
    std::optional<root::vec3> scale;
    std::optional<root::vec3> rotation;
};

enum class level_state
{
    unloaded = 0,
    loaded,
    render_loaded
};

class level : public container
{
public:
    friend class level_manager;

    level(const utils::id& id);
    ~level();

    root::game_object*
    find_game_object(const utils::id& id);

    root::component*
    find_component(const utils::id& id);

    template <typename T>
    auto
    spawn_object_from_proto(const utils::id& proto_id,
                            const utils::id& id,
                            const spawn_parameters& prms)
    {
        return spawn_object_impl(proto_id, id, prms)->as<T>();
    }

    template <typename T>
    auto
    spawn_object_as_clone(const utils::id& proto_id,
                          const utils::id& id,
                          const spawn_parameters& prms)
    {
        return spawn_object_as_clone_impl(proto_id, id, prms)->as<T>();
    }

    template <typename T>
    auto
    spawn_object(const utils::id& id, const typename T::construct_params& prms)
    {
        return spawn_object_impl(T::AR_TYPE_id(), id, prms)->as<T>();
    }

    void
    tick(float dt);

    game_objects_cache&
    get_game_objects()
    {
        return m_instance_local_cs.game_objects;
    }

    const std::vector<utils::id>&
    get_package_ids() const
    {
        return m_package_ids;
    }

    object_load_context&
    get_load_context() const
    {
        return *m_occ;
    }

    void
    unregister_objects();

    void
    unload();

    level_state
    get_state() const
    {
        return m_state;
    }

private:
    root::smart_object*
    spawn_object_impl(const utils::id& proto_id, const utils::id& id, const spawn_parameters& prms);

    root::smart_object*
    spawn_object_as_clone_impl(const utils::id& proto_id,
                               const utils::id& id,
                               const spawn_parameters& prms);

    root::smart_object*
    spawn_object_impl(const utils::id& proto_id,
                      const utils::id& id,
                      const root::smart_object::construct_params& p);

    level_state m_state = level_state::unloaded;
    vfs::backend* m_backend = nullptr;

    line_cache<root::game_object*> m_tickable_objects;

    std::vector<utils::id> m_package_ids;

    utils::id m_selected_directional_light_id;

public:
    void
    set_selected_directional_light(const utils::id& id)
    {
        m_selected_directional_light_id = id;
    }

    const utils::id&
    get_selected_directional_light_id() const
    {
        return m_selected_directional_light_id;
    }
};

}  // namespace core

}  // namespace kryga
