#pragma once

#include "model/model_minimal.h"
#include "model/smart_object.h"

#include <utils/singleton_instance.h>

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
struct empty_objects_cache
    : public singleton_instance<::agea::model::empty_objects_cache, empty_objects_cache>
{
};

}  // namespace glob
}  // namespace agea