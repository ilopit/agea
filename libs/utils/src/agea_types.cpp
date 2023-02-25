#include "utils/agea_types.h"

namespace agea
{
namespace utils
{

uint32_t
agea_type::size(agea_type::id t)
{
    switch (t)
    {
    case agea::utils::agea_type::id::t_i8:
    case agea::utils::agea_type::id::t_u8:
        return 1;
    case agea::utils::agea_type::id::t_i16:
    case agea::utils::agea_type::id::t_u16:
        return 2;
    case agea::utils::agea_type::id::t_i32:
    case agea::utils::agea_type::id::t_u32:
    case agea::utils::agea_type::id::t_f:
    case agea::utils::agea_type::id::t_color:
        return 4;
    case agea::utils::agea_type::id::t_i64:
    case agea::utils::agea_type::id::t_u64:
    case agea::utils::agea_type::id::t_d:
    case agea::utils::agea_type::id::t_vec2:
        return 8;
    case agea::utils::agea_type::id::t_vec3:
        return 12;
    case agea::utils::agea_type::id::t_vec4:
    case agea::utils::agea_type::id::t_mat2:
        return 16;
    case agea::utils::agea_type::id::t_mat3:
        return 4 * 9;
    case agea::utils::agea_type::id::t_mat4:
        return 16 * 4;
    case agea::utils::agea_type::id::t_nan:
    case agea::utils::agea_type::id::t_id:
    case agea::utils::agea_type::id::t_bool:
    case agea::utils::agea_type::id::t_txt:
    case agea::utils::agea_type::id::t_mat:
    case agea::utils::agea_type::id::t_msh:
    case agea::utils::agea_type::id::t_se:
    case agea::utils::agea_type::id::t_obj:
    case agea::utils::agea_type::id::t_com:
    case agea::utils::agea_type::id::t_buf:
    default:
        break;
    }

    return 0;
};

}  // namespace utils
}  // namespace agea