file(GLOB LIB_SRC "public/include/rpc/*.h" "private/src/*.h" "private/src/*.cpp")

add_library(rpc STATIC
   ${LIB_SRC}
)

target_link_libraries(rpc
    PUBLIC
        kryga::utils
        jsoncpp_static
        Boost::asio
)

kryga_finalize_library(rpc)
