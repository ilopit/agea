#pragma once

#include <cstdint>
#include <string>

namespace agea
{
namespace ipc
{
struct construct_params
{
    std::string path_to_binary;
    std::string arguments;
    std::string working_dir;
};

bool
run_binary(construct_params params, std::uint64_t& result_code);
}  // namespace ipc

}  // namespace agea
