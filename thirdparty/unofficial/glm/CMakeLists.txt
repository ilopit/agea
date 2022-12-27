add_library(glm_unofficial STATIC glm_unofficial/glm.cpp)

add_library(agea::glm_unofficial ALIAS glm_unofficial)

target_include_directories(glm_unofficial PUBLIC ".")
target_link_libraries(glm_unofficial PUBLIC glm::glm)