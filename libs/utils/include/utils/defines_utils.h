#pragma once

#define AGEA_return_nok(cond) \
    if (!(cond))              \
    {                         \
        return false;         \
    }

#define AGEA_stringify_impl(s) #s
#define AGEA_stringify(s) AGEA_stringify_impl(s)

#define AGEA_concat_impl2(x, y) x##y
#define AGEA_concat2(x, y) AGEA_concat_impl2(x, y)

#define AGEA_concat_impl3(x, y, z) x##y##z
#define AGEA_concat3(x, y, z) AGEA_concat_impl3(x, y, z)

#define AGEA_gen_class_cd_default(c) \
    c## ::##c() = default;           \
    c## ::##~c() = default;

#define AGEA_gen_class_non_copyable(c) \
    c(const c&) = delete; \
    c& operator=(const c&) = delete

#define AGEA_gen_class_non_moveable(c) \
    c(c&&) = delete; \
    c& operator=(c&&)) = delete

#define AGEA_gen_class_non_copyble_non_moveable(c) \
AGEA_gen_class_non_copyable(c); \
AGEA_gen_class_non_moveable(c);

#define AGEA_unused(val) (void)(val)