#pragma once

#include "core/agea_minimal.h"

#include "utils/weird_singletone.h"

namespace agea
{
namespace model
{

class smart_object;

class class_objects_cache
{
public:
    struct class_object_context
    {
        std::shared_ptr<smart_object> obj;
        std::string path;
        int order = 0;
    };

    smart_object*
    get(const std::string& class_id);

    void
    insert(std::shared_ptr<smart_object> obj, const std::string& path, int order);

    void
    insert(std::shared_ptr<smart_object> obj);

    bool
    exists(const std::string& class_id);

    size_t
    size()
    {
        return m_objects.size();
    }

    std::vector<class_object_context>
    get_order();

protected:
    std::unordered_map<std::string, class_object_context> m_objects;
};

}  // namespace model

namespace glob
{
struct class_objects_cache : public weird_singleton<::agea::model::class_objects_cache>
{
};
}  // namespace glob
}  // namespace agea