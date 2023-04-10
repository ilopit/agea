
file(GLOB DEMO_SOURCES "include/demo/*.h" "src/*.cpp")

file(GLOB GENERATED_SRC "${CMAKE_BINARY_DIR}/agea_generated/demo/*.cpp")

source_group("demo_sources" FILES  ${DEMO_SOURCES})

add_library(demo STATIC
   ${DEMO_SOURCES}         
   
   ${GENERATED_SRC}
)

target_compile_options(demo PRIVATE /bigobj)

target_link_libraries(demo PUBLIC
   agea::ar
   agea::utils
   agea::glm_unofficial
   agea::model
   agea::root
   agea::serialization
   agea::resource_locator

   lua_static
   agea::sol2_unofficial
)

agea_finalize_library(demo)

target_include_directories(demo PUBLIC ${CMAKE_BINARY_DIR}/agea_generated)

add_custom_target(generate.demo.ar ALL)
agea_ar_target(generate.demo.ar demo " " 100)

