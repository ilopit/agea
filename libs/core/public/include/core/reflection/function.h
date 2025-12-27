#pragma once

#include <string>
#include <vector>

namespace agea
{
namespace reflection
{

struct reflection_type;

class function
{
public:
    std::string name;

    std::vector<reflection_type*> args;
    reflection_type* return_type = nullptr;

    std::string category;
};
}  // namespace reflection
}  // namespace agea