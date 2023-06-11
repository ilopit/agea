#include "error_handling/error_handling.h"

namespace agea
{
const char*
to_cstr(result_code rc)
{
    switch (rc)
    {
        // clang-format off
    case result_code::nav: return "nav";
    case result_code::ok: return "ok";
    case result_code::proto_doesnt_exist: return "proto_doesnt_exist";
    case result_code::doesnt_exist: return "doesnt_exist";
    case result_code::serialization_error: return "serialization_error";
    case result_code::path_not_found: return "path_not_found";
    case result_code::id_not_found: return "id_not_found";
        // clang-format on
    default:
        break;
    }
    return "";
}
}  // namespace agea
