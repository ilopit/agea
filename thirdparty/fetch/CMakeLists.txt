include(FetchContent)

FetchContent_Declare(
  Boost
  URL https://github.com/boostorg/boost/releases/download/boost-1.81.0/boost-1.81.0.tar.xz
  URL_MD5 6cf0cdd797bca685910d527ae3c08cb3
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)

set(BOOST_INCLUDE_LIBRARIES beast url)
FetchContent_MakeAvailable(Boost)
