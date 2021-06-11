cmake_minimum_required (VERSION 3.16)
project(ndn_project)

link_directories(../libs/hiredis)
link_directories(../libs/libuv/build)
link_directories(../libs/redis-plus-plus/build)

include_directories(../libs/spdlog/include)
include_directories(../libs/rapidjson/include)

link_libraries(uv)
link_libraries(ssl)
link_libraries(crypto)
link_libraries(hiredis)
link_libraries(redis++)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)


file(GLOB core_src ./core/*.cxx)
file(GLOB packet_src ./packet/*.cxx)

add_executable(server server.cxx util.cxx ${core_src} ${packet_src})
add_executable(router router.cxx util.cxx ${core_src} ${packet_src})
add_executable(sha256test test/sha256test.cxx util.cxx)
add_executable(rsatest test/rsatest.cxx util.cxx)
add_executable(redistest test/redistest.cxx)