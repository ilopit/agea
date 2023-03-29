#include "model/smart_object.h"

namespace agea
{
namespace model
{

AGEA_gen_class_cd_default(smart_object);

bool
smart_object::post_construct()
{
    return true;
}

bool
smart_object::post_load()
{
    return true;
}

}  // namespace model
}  // namespace agea
