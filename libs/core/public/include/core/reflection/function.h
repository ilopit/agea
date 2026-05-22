#pragma once

#include "core/model_minimal.h"

#include <string>
#include <vector>

namespace Json
{
class Value;
}

namespace kryga
{

namespace root
{
class smart_object;
}

namespace reflection
{

struct reflection_type;

struct function_invoke_context
{
    root::smart_object* obj = nullptr;
    const Json::Value* args = nullptr;
    Json::Value* result = nullptr;
};

using function_invoke_handler = result_code (*)(function_invoke_context&);

class function
{
public:
    std::string name;
    std::string category;
    std::string mcp_hint;

    std::vector<reflection_type*> arg_types;
    reflection_type* return_type = nullptr;

    function_invoke_handler invoke = nullptr;
};
}  // namespace reflection
}  // namespace kryga
