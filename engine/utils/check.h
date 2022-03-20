#pragma once

#include <cassert>

#define AGEA_cond(condition, msg) assert((condition) && msg)
#define AGEA_never(msg) assert(false && msg)
