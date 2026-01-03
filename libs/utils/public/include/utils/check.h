#pragma once

#include <cassert>

#define KRG_check(condition, msg) assert((condition) && msg)
#define KRG_never(msg) assert(false && msg)
#define KRG_not_implemented KRG_never("Not Implemented!")
#define KRG_deprecate KRG_never("Not Implemented")
