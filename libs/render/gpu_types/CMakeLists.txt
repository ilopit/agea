file(GLOB LIB_SRC "public/include/gpu_types/*.h" "private/src/*.cpp")

add_library(gpu_types STATIC
   ${LIB_SRC}
)

target_link_libraries(gpu_types PUBLIC
    kryga::glm_unofficial
    kryga::utils
)

target_precompile_headers(gpu_types PUBLIC
    "$<$<COMPILE_LANGUAGE:CXX>:<glm/glm.hpp$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<glm/gtc/quaternion.hpp$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<glm/gtc/matrix_transform.hpp$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<glm/gtc/type_ptr.hpp$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<glm/gtx/quaternion.hpp$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<glm/gtx/transform.hpp$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<glm/gtx/rotate_vector.hpp$<ANGLE-R>>"
)

kryga_finalize_library(gpu_types)
