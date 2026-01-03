#pragma once

#define KRG_return_false(cond) \
    if (!(cond))                \
    {                           \
        return false;           \
    }

#define KRG_return_nok(rc)    \
    if (rc != result_code::ok) \
    {                          \
        return rc;             \
    }

#define KRG_stringify_impl(s) #s
#define KRG_stringify(s) KRG_stringify_impl(s)

#define KRG_concat_impl2(x, y) x##y
#define KRG_concat2(x, y) KRG_concat_impl2(x, y)

#define KRG_concat_impl3(x, y, z) x##y##z
#define KRG_concat3(x, y, z) KRG_concat_impl3(x, y, z)

#define KRG_gen_class_cd_default(c) \
    c## ::##c() = default;           \
    c## ::##~c() = default;

#define KRG_gen_class_non_copyable(c) \
    c(const c&) = delete;              \
    c& operator=(const c&) = delete

#define KRG_gen_class_non_moveable(c) \
    c(c&&) = delete;                   \
    c& operator=(c&&) = delete

#define KRG_gen_class_non_copyble_non_moveable(c) \
    KRG_gen_class_non_copyable(c);                \
    KRG_gen_class_non_moveable(c);

#define KRG_unused(val) (void)(val)