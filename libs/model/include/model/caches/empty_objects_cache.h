#pragma once

#include "model/model_minimal.h"

#include "utils/weird_singletone.h"
#include "model/smart_object.h"

namespace agea
{
namespace model
{
class empty_objects_cache
{
public:
    model::smart_object*
    get(const utils::id& type_id)
    {
        auto itr = m_meshes.find(type_id);

        return itr != m_meshes.end() ? itr->second.get() : nullptr;
    }

    void
    insert(const utils::id& type_id, const std::shared_ptr<model::smart_object>& obj)
    {
        m_meshes.insert({type_id, obj});
    }

protected:
    std::unordered_map<utils::id, std::shared_ptr<model::smart_object>> m_meshes;
};

}  // namespace model

namespace glob
{
struct empty_objects_cache : public selfcleanable_singleton<::agea::model::empty_objects_cache>
{
    static singletone_autodeleter s_closure;
};

}  // namespace glob
}  // namespace agea