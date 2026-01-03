#pragma once

namespace kryga
{
enum class result_code
{
    nav = 0,
    ok,
    fallback,
    failed,
    compilation_failed,
    proto_doesnt_exist,
    doesnt_exist,
    serialization_error,
    path_not_found,
    id_not_found
};

const char*
to_cstr(result_code rc);
}  // namespace kryga