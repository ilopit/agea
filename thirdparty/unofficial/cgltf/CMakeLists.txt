FetchContent_GetProperties(cgltf)

add_library(cgltf_unofficial STATIC cgltf_impl.c)

target_include_directories(cgltf_unofficial PUBLIC "${cgltf_SOURCE_DIR}")

add_library(kryga::cgltf ALIAS cgltf_unofficial)
