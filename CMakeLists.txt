cmake_minimum_required(VERSION 3.5.0)

project(desk_cast CXX)

find_package(OpenSSL REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(absl CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD 20)
# set(CMAKE_CXX_FLAGS "-g -Wall -Werror")
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
set(OPENSSL_USE_STATIC_LIBS TRUE)
set(Protobuf_USE_STATIC_LIBS ON)

add_subdirectory(proto)

include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(include)


file(GLOB_RECURSE SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} 
    cast_proto 
    protobuf::libprotobuf
    protobuf::libprotobuf-lite
    ${OPENSSL_LIBRARIES}
)
