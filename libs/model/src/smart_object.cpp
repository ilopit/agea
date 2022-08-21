#include "model/smart_object.h"

namespace agea
{
namespace model
{

AGEA_gen_class_cd_default(smart_object);

bool
smart_object::post_construct()
{
    set_state(smart_objet_state__constructed);
    return true;
}

}  // namespace model
}  // namespace agea
