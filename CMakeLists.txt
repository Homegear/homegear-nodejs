cmake_minimum_required(VERSION 3.17)
project(homegear_nodejs)

set(CMAKE_CXX_STANDARD 17)

include_directories("/usr/include/node")

add_library(homegear_nodejs homegear.cpp IpcClient.cpp IpcClient.h HomegearObject.cpp HomegearObject.h NapiVariableConverter.cpp NapiVariableConverter.h)
