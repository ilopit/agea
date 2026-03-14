add_library(glm_unofficial STATIC glm_unofficial/glm.cpp)

add_library(kryga::glm_unofficial ALIAS glm_unofficial)

target_include_directories(glm_unofficial PUBLIC ".")
target_link_libraries(glm_unofficial PUBLIC glm::glm)
target_compile_definitions(glm_unofficial PUBLIC
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_ENABLE_EXPERIMENTAL
)