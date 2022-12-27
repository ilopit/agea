#pragma once

#include "utils/path.h"

#include <cstdint>
#include <string>

namespace agea
{
namespace ipc
{
struct construct_params
{
    utils::path path_to_binary;
    std::string arguments;
    utils::path working_dir;
};

bool
run_binary(construct_params params, std::uint64_t& result_code);
}  // namespace ipc

}  // namespace agea
