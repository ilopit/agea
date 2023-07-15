#pragma once

#include <cassert>

#define AGEA_check(condition, msg) assert((condition) && msg)
#define AGEA_never(msg) assert(false && msg)
#define AGEA_not_implemented AGEA_never("Not Implemented!")
#define AGEA_depricate AGEA_never("Not Implemented")
