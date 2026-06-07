#pragma once

namespace kryga
{
// Kept deliberately dependency-free (leaf lib): utils/defines_utils.h references
// result_code, so error_handling must NOT depend on utils or a cycle forms.
// That's why this is a plain enum rather than KRG_declare_enum — the macro lives
// in utils and pulls <format>. If result_code ever needs string/log support,
// move the enum helper to a true leaf first.
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
    id_not_found,
    validation_error
};
}  // namespace kryga
