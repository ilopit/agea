#include "model/object_prototypes_registry.h"

#include "model/smart_object.h"

namespace agea
{
namespace model
{
void
smart_object_prototype_registry::get_registry(prototypes_list*& or)
{
    static prototypes_list s_objects_regestry;
    or = &s_objects_regestry;
}

std::shared_ptr<smart_object>
smart_object_prototype_registry::get_empty(const std::string& id)
{
    prototypes_list* or = nullptr;
    smart_object_prototype_registry::get_registry(or);

    return or->at(id)->META_create_empty_obj();
}

}  // namespace model
}  // namespace agea
