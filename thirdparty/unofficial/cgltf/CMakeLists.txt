add_library(cgltf_unofficial STATIC cgltf_impl.c)

target_include_directories(cgltf_unofficial PUBLIC "${PROJECT_SOURCE_DIR}/thirdparty/upstream/cgltf")

add_library(kryga::cgltf ALIAS cgltf_unofficial)
