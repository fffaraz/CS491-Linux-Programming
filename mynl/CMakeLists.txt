project(mynl)
cmake_minimum_required(VERSION 2.8)
aux_source_directory(. SRC_LIST)
add_executable(${PROJECT_NAME} ${SRC_LIST})
#SET(CMAKE_C_FLAGS "-std=gnu99")
SET(CMAKE_C_FLAGS "-std=gnu11")
